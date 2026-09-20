#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(__arm__) || defined(__TARGET_ARCH_ARM)
// Windows / Bare-metal ARM cross-compiler (STM32 build fix)
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

// ----------------------------------------------------------- structures & globals --
const char *SPEED_NAMES[] = {"SLOW", "MEDIUM", "FAST", "URGENT"};

#define MAX_SITUATIONS 32
#define SITUATION_LEN 32

typedef struct {
    char name[SITUATION_LEN];
    double scores[4];
} SituationScore;

SituationScore scoreboard[MAX_SITUATIONS];
int num_situations = 0;

// Helper: generate standard normal distribution random numbers (Box-Muller)
double random_gauss(double mean, double stddev) {
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    while (u1 <= 1e-15) u1 = (double)rand() / RAND_MAX; // Avoid log(0)
    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979323846 * u2);
    return z0 * stddev + mean;
}

// ---------------------------------------------------------------- brain --
void describe_situation(int heart_rate, int previous_heart_rate, char *output) {
    const char *risk;

    if (heart_rate == 0) {
        risk = "unknown";
    } else if (heart_rate > 140 || heart_rate < 40) {
        risk = "critical";
    } else if (heart_rate > 110 || heart_rate < 55) {
        risk = "elevated";
    } else if (heart_rate > 95) {
        risk = "watch";
    } else {
        risk = "stable";
    }

    bool changing_fast = abs(heart_rate - previous_heart_rate) > 3;
    const char *trend = changing_fast ? "changing" : "steady";

    snprintf(output, SITUATION_LEN, "%s_%s", risk, trend);
}

double* get_scores(const char *situation) {
    // Look up existing situation in scoreboard
    for (int i = 0; i < num_situations; i++) {
        if (strcmp(scoreboard[i].name, situation) == 0) {
            return scoreboard[i].scores;
        }
    }

    // Add new situation entry if space permits
    if (num_situations < MAX_SITUATIONS) {
        int idx = num_situations++;
        strncpy(scoreboard[idx].name, situation, SITUATION_LEN - 1);
        scoreboard[idx].name[SITUATION_LEN - 1] = '\0';

        int guess = 0;
        if (strncmp(situation, "critical", 8) == 0) {
            guess = 3;
        } else if (strncmp(situation, "elevated", 8) == 0 || strncmp(situation, "unknown", 7) == 0) {
            guess = 2;
        } else if (strncmp(situation, "watch", 5) == 0) {
            guess = 1;
        } else {
            guess = 0;
        }

        for (int j = 0; j < 4; j++) {
            scoreboard[idx].scores[j] = 0.0;
        }
        scoreboard[idx].scores[guess] = 1.0;

        return scoreboard[idx].scores;
    }

    return scoreboard[0].scores; // Fallback
}

int choose_speed(const char *situation, double explore_chance) {
    double *row = get_scores(situation);
    int speed = 0;

    double rand_val = (double)rand() / RAND_MAX;
    if (rand_val < explore_chance) {
        speed = rand() % 4;
    } else {
        double max_val = row[0];
        speed = 0;
        for (int i = 1; i < 4; i++) {
            if (row[i] > max_val) {
                max_val = row[i];
                speed = i;
            }
        }
    }

    // SAFETY RULE -- overrides scoreboard
    if (strncmp(situation, "critical", 8) == 0 && speed < 3) {
        speed = 3;
    } else if ((strncmp(situation, "elevated", 8) == 0 || strncmp(situation, "unknown", 7) == 0) && speed < 2) {
        speed = 2;
    }

    return speed;
}

void learn(const char *situation, int speed_used, bool event_happened) {
    double *row = get_scores(situation);
    double reward = 0.0;

    if (event_happened && speed_used < 2) {
        reward -= 10.0;
    } else if (event_happened) {
        reward += 2.0;
    } else {
        reward += (3 - speed_used) * 0.3;
    }

    double learning_rate = 0.2;
    row[speed_used] = row[speed_used] + learning_rate * (reward - row[speed_used]);
}

int compare_situations(const void *a, const void *b) {
    return strcmp(((SituationScore *)a)->name, ((SituationScore *)b)->name);
}

void print_scoreboard() {
    printf("\n%-20s %-10s\n", "Situation", "Best speed");
    printf("--------------------------------\n");

    // Sort entries alphabetically
    qsort(scoreboard, num_situations, sizeof(SituationScore), compare_situations);

    for (int i = 0; i < num_situations; i++) {
        double *row = scoreboard[i].scores;
        int best = 0;
        double max_val = row[0];
        for (int j = 1; j < 4; j++) {
            if (row[j] > max_val) {
                max_val = row[j];
                best = j;
            }
        }
        printf("%-20s %-10s\n", scoreboard[i].name, SPEED_NAMES[best]);
    }
}

// ------------------------------------------------------------- practice --
void practice_mode() {
    double heart_rate = 72.0;
    int target = 72;
    int ticks_left = 60;
    double battery = 100.0;
    int previous_heart_rate = 72;
    char situation[SITUATION_LEN];

    const double battery_drain[] = {0.002, 0.004, 0.008, 0.015};

    printf("Running 600 seconds of practice...\n\n");

    for (int second = 0; second < 600; second++) {
        ticks_left--;
        if (ticks_left <= 0) {
            ticks_left = 30 + rand() % 61; // random range 30 to 90
            if (((double)rand() / RAND_MAX) < 0.15) {
                target = 130 + rand() % 41; // 130 to 170
            } else {
                target = 65 + rand() % 21;  // 65 to 85
            }
        }

        heart_rate += (target - heart_rate) * 0.1 + random_gauss(0.0, 1.0);
        if (heart_rate < 30.0) heart_rate = 30.0;
        if (heart_rate > 200.0) heart_rate = 200.0;

        bool event_happened = (heart_rate > 130.0 || heart_rate < 45.0);

        int rounded_hr = (int)round(heart_rate);
        describe_situation(rounded_hr, previous_heart_rate, situation);

        int speed = choose_speed(situation, 0.2);
        battery -= battery_drain[speed];
        learn(situation, speed, event_happened);

        previous_heart_rate = rounded_hr;

        if (second % 60 == 0) {
            const char *flag = event_happened ? "  ** EVENT **" : "";
            printf("t=%4ds  HR=%5.1f  speed=%-7s  battery=%5.1f%%%s\n",
                   second, heart_rate, SPEED_NAMES[speed], battery, flag);
        }
    }

    print_scoreboard();
}

// ------------------------------------------------------------------ live --
void live_mode(const char *port) {
#if defined(_WIN32) || defined(__arm__) || defined(__TARGET_ARCH_ARM)
    printf("Live mode requires Linux POSIX terminal drivers.\n");
    return;
#else
    int serial_fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd == -1) {
        perror("Unable to open port");
        return;
    }

    struct termios options;
    tcgetattr(serial_fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD | CS8);
    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    options.c_lflag |= ICANON; // Line buffered mode
    tcsetattr(serial_fd, TCSANOW, &options);

    sleep(2); // Let Arduino finish resetting

    int previous_heart_rate = 0;
    int current_speed_sent = -1;
    char line[256];
    char situation[SITUATION_LEN];

    printf("Connected to %s. Press Ctrl+C to stop.\n\n", port);

    while (1) {
        int bytes_read = read(serial_fd, line, sizeof(line) - 1);
        if (bytes_read <= 0) continue;

        line[bytes_read] = '\0';

        // Trim trailing newline characters
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "DATA,", 5) != 0) {
            if (strlen(line) > 0) {
                printf("  [arduino] %s\n", line);
            }
            continue;
        }

        // Format: DATA,heartRate,leadOff,speedLevel
        int heart_rate = 0;
        int lead_off_val = 0;
        sscanf(line, "DATA,%d,%d", &heart_rate, &lead_off_val);
        bool lead_off = (lead_off_val == 1);

        describe_situation(heart_rate, previous_heart_rate, situation);
        int speed = choose_speed(situation, 0.2);

        if (lead_off) {
            if (speed < 1) speed = 1;
        }

        if (speed != current_speed_sent) {
            char speed_char = '0' + speed;
            write(serial_fd, &speed_char, 1);
            current_speed_sent = speed;
        }

        printf("HR=%3d  situation=%-18s -> %s%s\n",
               heart_rate, situation, SPEED_NAMES[speed],
               lead_off ? "  (lead off!)" : "");

        previous_heart_rate = heart_rate;
    }

    close(serial_fd);
#endif
}

// ------------------------------------------------------------------ main --
int main(int argc, char *argv[]) {
    srand(12345); // Fixed seed for bare-metal ARM microcontroller

    if (argc >= 2 && strcmp(argv[1], "practice") == 0) {
        practice_mode();
    } else if (argc >= 3 && strcmp(argv[1], "live") == 0) {
        live_mode(argv[2]);
    } else {
        printf("Usage:\n");
        printf("  agent practice\n");
        printf("  agent live COM3  (or /dev/ttyUSB0 on Linux)\n");
    }

    return 0;
}
