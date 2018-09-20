/*
 * File:    num_cores.c
 * Author:  osim082
 */

#include <stdio.h>
#include <unistd.h>

int main (int argc, char** argv) {
    // This reports the number of cores according to GNOME System Monitor.
    // though that number is wrong.
    int cpus = sysconf(_SC_NPROCESSORS_ONLN);
    printf("This machine has %i cores.\n", cpus);
}
