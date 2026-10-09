#include <stdio.h>
#include <unistd.h>

int main(void) {
    char buffer[32];
    ssize_t bytes;

    printf("Type something and press Enter\n");
    printf("Press Ctrl+D on an empty line to exit\n\n");
    fflush(stdout);

    while((bytes = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0){
        printf("Received %zd bytes: ", bytes);
        fflush(stdout);

        if(write(STDOUT_FILENO, buffer, (size_t)bytes) == -1){
            perror("Write failed");
            return 1;
        }

        printf("\n");
        fflush(stdout);
    }

    if(bytes == -1){
        perror("Read failed");
        return 1;
    }

    printf("End of input\n");
    return 0;
}