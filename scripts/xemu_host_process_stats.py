"""Read-only telemetry for one existing process; no enumeration or UI APIs."""
import time
import psutil


class HostProcessStats:
    def __init__(self, pid):
        self.process = psutil.Process(pid)
        self.created = self.process.create_time()
        self.affinity = self.process.cpu_affinity()

    def snapshot(self):
        # psutil's Process identity checks protect against PID reuse. Store
        # cumulative counters: derive CPU usage from elapsed host time later.
        if not self.process.is_running():
            raise psutil.NoSuchProcess(self.process.pid)
        with self.process.oneshot():
            return {'epoch': time.time(), 'created_epoch': self.created,
                    'cpu': self.process.cpu_times()._asdict(),
                    'memory': self.process.memory_info()._asdict(),
                    'io': self.process.io_counters()._asdict(),
                    'threads': [item._asdict() for item in self.process.threads()],
                    'host_available_bytes': psutil.virtual_memory().available,
                    'target_affinity': self.affinity,
                    'host_cpu_times': [item._asdict() for item in psutil.cpu_times(percpu=True)]}
