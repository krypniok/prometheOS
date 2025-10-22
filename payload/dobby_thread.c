/*
 * Simple Little C demo that exercises the kernel thread helpers exposed to
 * Dobby. The interpreter now runs inside its own kernel thread, so the script
 * can cooperate via sleep/stop checks without calling thread_yield() manually.
 * Run via "dobby dobby_thread.c" from the kernel prompt.
 */

int i;

main()
{
    int my_id;

    puts("[dobby_thread] cooperative demo\n");

    my_id = thread_current_id();
    puts("running on thread ");
    print(my_id);
    puts(" of ");
    print(thread_count());
    puts(" total\n\n");

    for (i = 0; i < 10; i = i + 1) {
        puts("tick ");
        print(i);
        puts(": active threads=");
        print(thread_count());
        puts(" stop? ");
        print(thread_should_stop());
        puts("\n");

        sleep(200);

        if (thread_should_stop()) {
            puts("stop request observed -> leaving loop\n");
            return 0;
        }
    }

    puts("done\n");
    return 0;
}
