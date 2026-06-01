import nt;
import print;

int main() {
    LARGE_INTEGER timeout;

    timeout.QuadPart = 0;

    while (true) {
        print("y\n");

        NtDelayExecution(true, &timeout);
    }

    return 0;
}
