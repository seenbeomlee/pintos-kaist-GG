/* Verifies that a single busy thread raises the load average to
   0.5 in 38 to 45 seconds.  The expected time is 42 seconds, as
   you can verify:
   perl -e '$i++,$a=(59*$a+1)/60while$a<=.5;print "$i\n"'

   Then, verifies that 10 seconds of inactivity drop the load
   average back below 0.5 again. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

void
test_mlfqs_load_1 (void) 
{
  int64_t start_time;
  int elapsed;
  int load_avg;
  
  // 버그있다. build 폴더에서 pintos -- -q run mlfqs-load-1 실행시, ACCERTION 위반으로 PANIC 에러가 뜸.
  // 이는 init.c 222 line에서 thread-mlfas = true가 되지 않기 때문인데, 
  // 무엇 떄문인지는 모르겠지만, 커널에 입력된 테스트 문구를 통해서는 else if (!strcmp (name, "-mlfqs")) 가 통과되지 않기 떄문으로 보임.
  // 일단 mlfqs-load-1로 아래의 문장을 추가함으로써 테스트가 정상적으로 PASS됨을 확인하였고,
  // 임시방편으로는 mlfqs 테스트 독자적으로 돌릴 때는 init.c의 parse_options 시작지점에서 thread_mlfqs = true;를 추가해주어야 할 듯.
  // thread_mlfqs = true;
  ASSERT (thread_mlfqs);

  msg ("spinning for up to 45 seconds, please wait...");

  start_time = timer_ticks ();
  for (;;) 
    {
      load_avg = thread_get_load_avg ();
      ASSERT (load_avg >= 0);
      elapsed = timer_elapsed (start_time) / TIMER_FREQ;
      if (load_avg > 100)
        fail ("load average is %d.%02d "
              "but should be between 0 and 1 (after %d seconds)",
              load_avg / 100, load_avg % 100, elapsed);
      else if (load_avg > 50)
        break;
      else if (elapsed > 45)
        fail ("load average stayed below 0.5 for more than 45 seconds");
    }

  if (elapsed < 38)
    fail ("load average took only %d seconds to rise above 0.5", elapsed);
  msg ("load average rose to 0.5 after %d seconds", elapsed);

  msg ("sleeping for another 10 seconds, please wait...");
  timer_sleep (TIMER_FREQ * 10);

  load_avg = thread_get_load_avg ();
  if (load_avg < 0)
    fail ("load average fell below 0");
  if (load_avg > 50)
    fail ("load average stayed above 0.5 for more than 10 seconds");
  msg ("load average fell back below 0.5 (to %d.%02d)",
       load_avg / 100, load_avg % 100);

  pass ();
}
