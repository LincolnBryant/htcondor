universe   = scheduler
executable = /bin/sleep
log = job_core_onexitrem-true_sched.log
output = job_core_onexitrem-true_sched.out
error = job_core_onexitrem-true_sched.err
on_exit_remove = (CurrentTime - QDate) > 60
Notification = NEVER
arguments  = 3
queue

