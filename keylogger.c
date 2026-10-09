#include <stdio.h>
#include <unistd.h>

int main(void) {
    char buffer[32];

    printf("Type something: ");
    fflush(stdout);

    ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));

    if(bytes == -1) {
        perror("Reading error");
        return 1;
    }

    printf("%zd bytes received", bytes);

    return 0;
}