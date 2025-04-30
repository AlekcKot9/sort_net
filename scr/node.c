// node.c — рабочий узел
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

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
    if (server_fd < 0) {
        perror("Ошибка создания сокета");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Ошибка привязки сокета");
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("Ошибка прослушивания");
        return 1;
    }

    printf("Узел: ожидание части файла на порту %d...\n", PORT);
    client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (client_socket < 0) {
        perror("Ошибка accept");
        return 1;
    }

    // Считать ID узла
    char id_buf[16] = {0};
    int id_len = 0;
    while (recv(client_socket, &id_buf[id_len], 1, 0) > 0 && id_buf[id_len] != '\n') {
        id_len++;
        if (id_len >= 15) break;
    }
    id_buf[id_len] = 0;
    int node_id = atoi(id_buf);

    // Читаем входной текст
    size_t total_len = 0;
    size_t buffer_cap = BUFFER_SIZE;
    char *text = malloc(buffer_cap);
    if (!text) {
        perror("malloc");
        return 1;
    }

    ssize_t bytes;
    while ((bytes = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (total_len + bytes >= buffer_cap) {
            buffer_cap *= 2;
            text = realloc(text, buffer_cap);
            if (!text) {
                perror("realloc");
                return 1;
            }
        }
        memcpy(text + total_len, buffer, bytes);
        total_len += bytes;
    }
    text[total_len] = '\0';

    // Разделить на слова
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

    // Сортировка
    qsort(words, word_count, sizeof(char *), compare);

    // Отправка: всё в одну строку
    for (size_t i = 0; i < word_count; ++i) {
        send(client_socket, words[i], strlen(words[i]), 0);
        if (i < word_count - 1) {
            send(client_socket, " ", 1, 0);
        }
        free(words[i]);
    }
    free(words);

    printf("Узел #%d: отправка завершена (в одной строке)\n", node_id);
    close(client_socket);
    close(server_fd);
    return 0;
}

