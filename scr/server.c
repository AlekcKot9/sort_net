#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <ctype.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_NODES 10
#define MAX_WORDS 100000

const char* node_ips[MAX_NODES] = {
    "192.168.192.16",
    "192.168.192.16"
};

int node_ports[MAX_NODES] = {
    9091,
    9092
};

int node_count = 2;

int connect_to_node(const char* ip, int port) {
    int sock;
    struct sockaddr_in node_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Ошибка создания сокета узла");
        return -1;
    }

    node_addr.sin_family = AF_INET;
    node_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &node_addr.sin_addr) <= 0) {
        perror("Неверный IP узла");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&node_addr, sizeof(node_addr)) < 0) {
        perror("Не удалось подключиться к узлу");
        close(sock);
        return -1;
    }

    return sock;
}

#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024

void split_file_by_size(const char* filename, int parts) {
    FILE* input = fopen(filename, "r");
    if (!input) {
        perror("Не удалось открыть файл для чтения");
        exit(1);
    }

    // Получаем размер файла
    fseek(input, 0, SEEK_END);
    long filesize = ftell(input);
    rewind(input);

    // Загружаем весь файл в память
    char* content = malloc(filesize + 1);
    if (!content) {
        perror("Ошибка выделения памяти");
        fclose(input);
        exit(1);
    }
    fread(content, 1, filesize, input);
    content[filesize] = '\0';
    fclose(input);

    long approx_size = filesize / parts;

    int start = 0;
    for (int i = 0; i < parts; ++i) {
        int end = start + approx_size;

        // Не выходим за пределы файла
        if (end >= filesize || i == parts - 1) {
            end = filesize;
        } else {
            // Сдвигаем `end` до ближайшего пробела вправо, чтобы не разрезать слово
            while (end < filesize && content[end] != ' ') {
                ++end;
            }
        }

        char outname[64];
        snprintf(outname, sizeof(outname), "part_%d.txt", i);
        FILE* out = fopen(outname, "w");
        if (!out) {
            perror("Ошибка открытия выходного файла");
            free(content);
            exit(1);
        }

        fwrite(content + start, 1, end - start, out);
        fclose(out);

        start = end + 1;  // пропускаем пробел
    }

    free(content);
}


int compare_words(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    FILE *file = fopen("received_file.txt", "wb");
    if (!file) {
        perror("Не удалось открыть файл для записи");
        return 1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 1);

    printf("Сервер: ожидание файла от клиента...\n");
    client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    ssize_t bytes_received;
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }
    fclose(file);
    printf("Сервер: файл получен.\n");

    // Разделение
    split_file_by_size("received_file.txt", node_count);

    // Отправка частей узлам и получение результата
    for (int i = 0; i < node_count; ++i) {
        int node_sock = connect_to_node(node_ips[i], node_ports[i]);
        if (node_sock < 0) continue;

        char partname[64];
        snprintf(partname, sizeof(partname), "part_%d.txt", i);
        FILE* part = fopen(partname, "rb");
        if (!part) {
            perror("Не удалось открыть часть файла");
            close(node_sock);
            continue;
        }

        // Отправка ID
        char id_buf[16];
        snprintf(id_buf, sizeof(id_buf), "%d\n", i);
        send(node_sock, id_buf, strlen(id_buf), 0);
        usleep(10000);  // Пауза

        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, part)) > 0) {
            send(node_sock, buffer, bytes_read, 0);
        }
        shutdown(node_sock, SHUT_WR);
        fclose(part);

        // Приём отсортированного результата
        char sortedname[64];
        snprintf(sortedname, sizeof(sortedname), "sorted_%d.txt", i);
        FILE* sorted = fopen(sortedname, "wb");
        if (!sorted) {
            perror("Не удалось создать файл sorted_*");
            close(node_sock);
            continue;
        }
        while ((bytes_received = recv(node_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, bytes_received, sorted);
        }
        fclose(sorted);
        close(node_sock);
    }

    // Объединение всех sorted_* в один список слов
    char* words[MAX_WORDS];
    int total_words = 0;

    for (int i = 0; i < node_count; ++i) {
        char sortedname[64];
        snprintf(sortedname, sizeof(sortedname), "sorted_%d.txt", i);
        FILE* f = fopen(sortedname, "r");
        if (!f) continue;

        while (fscanf(f, "%s", buffer) == 1) {
            words[total_words] = strdup(buffer);
            if (++total_words >= MAX_WORDS) break;
        }
        fclose(f);
    }

    // Финальная сортировка
    qsort(words, total_words, sizeof(char*), compare_words);

    // Сохранение финального результата
    FILE* final = fopen("final_sorted.txt", "w");
    if (!final) {
        perror("Ошибка открытия финального файла");
        return 1;
    }

    for (int i = 0; i < total_words; ++i) {
        fprintf(final, "%s", words[i]);
        if (i < total_words - 1) fprintf(final, " ");
        free(words[i]);
    }
    fclose(final);

    // Отправка клиенту
    final = fopen("final_sorted.txt", "rb");
    if (!final) {
        perror("Не удалось открыть финальный файл для клиента");
        return 1;
    }

    while ((bytes_received = fread(buffer, 1, BUFFER_SIZE, final)) > 0) {
        send(client_socket, buffer, bytes_received, 0);
    }

    printf("Сервер: отсортированный файл отправлен клиенту.\n");

    fclose(final);
    close(client_socket);
    close(server_fd);
    return 0;
}

