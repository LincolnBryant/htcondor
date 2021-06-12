#!/usr/bin/env pytest

import logging

import htcondor

from ornithology import (
    config,
    standup,
    action,
    Condor
)

from libcmr import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


monitor_period = 5
resources = {
    "SQUID": 4,
    "TAKO": 4,
}
usages = {
    "SQUID": [5, 1, 9, 4],
    "TAKO": [500, 100, 900, 400],
}
peaks = {
    "SQUID": [
        [ 51, 51, 91, 11, 41, 41 ],
        [ 42, 42, 92, 12, 52, 52 ],
        [ 53, 53, 13, 93, 43, 43 ],
        [ 44, 44, 14, 94, 54, 54 ],
    ],
    "TAKO": [
        [ 5100, 5100, 9100, 1100, 4100, 4100 ],
        [ 4200, 4200, 9200, 1200, 5200, 5200 ],
        [ 5300, 5300, 1300, 9300, 4300, 4300 ],
        [ 4400, 4400, 1400, 9400, 5400, 5400 ],
    ],
}


@config(
    params={
        "SQUIDsAndTAKOsUsageAndMemory": {
            "config": {
                "NUM_CPUS": "16",
                "NUM_SLOTS": "16",
                "ADVERTISE_CMR_UPTIME_SECONDS": "TRUE",

                "MACHINE_RESOURCE_INVENTORY_SQUIDs": "$(TEST_DIR)/SQUID-discovery.py",
                "STARTD_CRON_SQUIDs_MONITOR_EXECUTABLE": "$(TEST_DIR)/SQUID-monitor.py",
                "STARTD_CRON_SQUIDs_MONITOR_MODE": "periodic",
                "STARTD_CRON_SQUIDs_MONITOR_PERIOD": str(monitor_period),
                "STARTD_CRON_SQUIDs_MONITOR_METRICS": "SUM:SQUIDs, PEAK:SQUIDsMemory",

                "MACHINE_RESOURCE_INVENTORY_TAKOs": "$(TEST_DIR)/TAKO-discovery.py",
                "STARTD_CRON_TAKOs_MONITOR_EXECUTABLE": "$(TEST_DIR)/TAKO-monitor.py",
                "STARTD_CRON_TAKOs_MONITOR_MODE": "periodic",
                "STARTD_CRON_TAKOs_MONITOR_PERIOD": str(monitor_period),
                "STARTD_CRON_TAKOs_MONITOR_METRICS": "SUM:TAKOs, PEAK:TAKOsMemory",

                "STARTD_CRON_JOBLIST": "$(STARTD_CRON_JOBLIST) SQUIDs_MONITOR TAKOs_MONITOR",
            },
        },
    }
)
def the_config(request):
    return request.param


@config
def slot_config(the_config):
    return the_config["config"]


@config
def num_resources():
    nr = next(iter(resources.values()))
    assert all(number == nr for number in resources.values())
    return nr


@standup
def condor(test_dir, slot_config):
    for resource in resources.keys():
        sequence = { f"{resource}{i}": j for i, j in enumerate(usages[resource]) }
        discovery_script = format_script(discovery_script_for(resource, sequence))
        write_file(test_dir / f"{resource}-discovery.py", discovery_script)

        sequences = { f"{resource}{i}": j for i, j in enumerate(peaks[resource]) }
        monitor_script = both_monitor_script(resource, sequence, sequences)
        write_file(test_dir / f"{resource}-monitor.py", monitor_script)

    with Condor(
        local_dir=test_dir / "condor",
        config={**slot_config, "TEST_DIR": test_dir.as_posix()},
    ) as condor:
        yield condor


def the_job(test_dir, resources):
    job_script = format_script( "#!/usr/bin/python3\n" + textwrap.dedent("""
        import os
        import sys
        import time

        elapsed = 0;
        while elapsed < int(sys.argv[1]):""" +

        "".join( f"""
            os.system('condor_status -ads ${{_CONDOR_SCRATCH_DIR}}/.update.ad -af Assigned{resource}s {resource}sMemoryUsage')
        """ for resource in resources
        ) +

        """
            time.sleep(1)
            elapsed += 1
        """)
    )

    script_file = test_dir / "poll-memory.py"
    write_file(script_file, job_script)

    job_spec = {
                "executable": script_file.as_posix(),
                "arguments": "17",
                "log": (test_dir / "events.log").as_posix(),
                "output": (test_dir / "poll-memory.$(Cluster).$(Process).out").as_posix(),
                "error": (test_dir / "poll-memory.$(Cluster).$(Process).err").as_posix(),
                "getenv": "true",
                "LeaveJobInQueue": "true",
    }

    for resource in resources:
        job_spec[f"request_{resource}s"] = "1"

    return job_spec


@action
def handle(test_dir, condor, num_resources):
    handle = condor.submit(
        description=the_job(test_dir, resources.keys()),
        count=num_resources * 2
    )

    assert(handle.wait(verbose=True, timeout=180))
    assert(condor.job_queue.wait_for_job_completion(handle.job_ids))

    yield handle

    handle.remove()


@action
def num_jobs_running_history(condor, handle, num_resources):
    return track_quantity(
        condor.job_queue.filter(lambda j, e: j in handle.job_ids),
        increment_condition=lambda id_event: id_event[-1]
            == SetJobStatus(JobStatus.RUNNING),
        decrement_condition=lambda id_event: id_event[-1]
            == SetJobStatus(JobStatus.COMPLETED),
        max_quantity=num_resources,
        expected_quantity=num_resources,
    )

class TestCustomMachineResources:

    def test_correct_number_of_resources_assigned(self, condor):
        for resource, number in resources.items():
            result = condor.status(
                ad_type=htcondor.AdTypes.Startd, projection=["SlotID", f"Assigned{resource}s"]
            )

            assert len([ad for ad in result if f"Assigned{resource}s" in ad]) == number

    def test_enough_jobs_running(
        self, num_jobs_running_history, num_resources
    ):
        assert num_resources in num_jobs_running_history

    def test_never_too_many_jobs_running(
        self, num_jobs_running_history, num_resources
    ):
        assert max(num_jobs_running_history) <= num_resources

    def test_correct_uptimes_from_monitors(self, condor, handle):
        for resource in resources.keys():
            sequence = { f"{resource}{i}": j for i, j in enumerate(usages[resource]) }
            sum_check_correct_uptimes(condor, handle, resource, sequence)

    def test_correct_peaks_from_monitors(self, condor, handle):
        for resource in resources.keys():
            sequences = { f"{resource}{i}": j for i, j in enumerate(peaks[resource]) }
            peak_check_correct_uptimes(condor, handle, resource, sequences)
