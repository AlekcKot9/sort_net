#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "192.168.192.16"  // <-- Укажи IP сервера
#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <путь к файлу>\n", argv[0]);
        return 1;
    }

    const char* file_path = argv[1];  // Путь к файлу, переданный как аргумент
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("Не удалось открыть файл");
        return 1;
    }

    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Ошибка создания сокета");
        fclose(file);
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Неверный IP-адрес сервера");
        fclose(file);
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Ошибка подключения");
        fclose(file);
        close(sock);
        return 1;
    }

    // Отправка файла
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(sock, buffer, bytes_read, 0) < 0) {
            perror("Ошибка отправки файла");
            fclose(file);
            close(sock);
            return 1;
        }
    }

    printf("Файл отправлен успешно\n");
    fclose(file);

    // Завершить отправку, но оставить сокет открытым для чтения
    shutdown(sock, SHUT_WR);

    // Открываем файл для записи отсортированных данных
    FILE *sorted = fopen("sorted_file", "wb");
    if (!sorted) {
        perror("Не удалось открыть файл для записи отсортированных данных");
        close(sock);
        return 1;
    }

    // Приём отсортированного файла
    ssize_t received;
    while ((received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, received, sorted);
    }

    printf("Отсортированный файл получен и сохранён в 'sorted_file'\n");

    fclose(sorted);
    close(sock);
    return 0;
}

