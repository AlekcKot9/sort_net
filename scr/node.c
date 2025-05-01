#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_THREADS 4

typedef struct {
    char **words;
    int start;
    int end;
} SortArgs;

int compare(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
}

void clean_word(char *word) {
    char *src = word, *dst = word;
    while (*src) {
        if (!ispunct((unsigned char)*src)) {
            *dst++ = tolower((unsigned char)*src);
        }
        src++;
    }
    *dst = '\0';
}

void *thread_sort(void *arg) {
    SortArgs *args = (SortArgs *)arg;
    qsort(args->words + args->start, args->end - args->start, sizeof(char *), compare);
    return NULL;
}

char **multiway_merge(char **words, int total, int parts, int *boundaries, int *result_count) {
    char **result = malloc(total * sizeof(char *));
    int *indexes = calloc(parts, sizeof(int));
    int *ends = malloc(parts * sizeof(int));
    for (int i = 0; i < parts; ++i)
        ends[i] = boundaries[i + 1];

    int pos = 0;
    while (1) {
        int min_idx = -1;
        for (int i = 0; i < parts; ++i) {
            int idx = boundaries[i] + indexes[i];
            if (idx >= ends[i]) continue;
            if (min_idx == -1 ||
                strcmp(words[idx], words[boundaries[min_idx] + indexes[min_idx]]) < 0) {
                min_idx = i;
            }
        }
        if (min_idx == -1) break;
        result[pos++] = words[boundaries[min_idx] + indexes[min_idx]++];
    }
    *result_count = pos;
    free(indexes);
    free(ends);
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <порт>\n", argv[0]);
        return 1;
    }

    int PORT = atoi(argv[1]);
    int server_fd, client_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];
    char **words = NULL;
    size_t word_count = 0;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 1);

    printf("Узел: ожидание части файла на порту %d...\n", PORT);
    client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    // Считать ID узла
    char id_buf[16] = {0};
    int id_len = 0;
    while (recv(client_socket, &id_buf[id_len], 1, 0) > 0 && id_buf[id_len] != '\n') {
        id_len++;
        if (id_len >= 15) break;
    }
    id_buf[id_len] = 0;
    int node_id = atoi(id_buf);

    // Получить текст
    size_t total_len = 0, buffer_cap = BUFFER_SIZE;
    char *text = malloc(buffer_cap);
    ssize_t bytes;
    while ((bytes = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (total_len + bytes >= buffer_cap) {
            buffer_cap *= 2;
            text = realloc(text, buffer_cap);
        }
        memcpy(text + total_len, buffer, bytes);
        total_len += bytes;
    }
    text[total_len] = '\0';

    // Токенизация
    char *token = strtok(text, " \n\r\t");
    while (token) {
        clean_word(token);
        if (strlen(token) > 0) {
            words = realloc(words, (word_count + 1) * sizeof(char *));
            words[word_count++] = strdup(token);
        }
        token = strtok(NULL, " \n\r\t");
    }
    free(text);

    // Разделим на части
    int threads_used = (word_count < MAX_THREADS) ? word_count : MAX_THREADS;
    pthread_t threads[threads_used];
    SortArgs args[threads_used];
    int boundaries[threads_used + 1];

    for (int i = 0; i <= threads_used; ++i)
        boundaries[i] = i * word_count / threads_used;

    for (int i = 0; i < threads_used; ++i) {
        args[i].words = words;
        args[i].start = boundaries[i];
        args[i].end = boundaries[i + 1];
        pthread_create(&threads[i], NULL, thread_sort, &args[i]);
    }

    for (int i = 0; i < threads_used; ++i)
        pthread_join(threads[i], NULL);

    // Слияние
    int result_count = 0;
    char **sorted = multiway_merge(words, word_count, threads_used, boundaries, &result_count);

    // Отправка результата
    for (int i = 0; i < result_count; ++i) {
        send(client_socket, sorted[i], strlen(sorted[i]), 0);
        if (i < result_count - 1) send(client_socket, " ", 1, 0);
        free(sorted[i]);
    }
    free(sorted);
    free(words);

    printf("Узел #%d: отправка завершена (многопоточная сортировка)\n", node_id);
    close(client_socket);
    close(server_fd);
    return 0;
}
