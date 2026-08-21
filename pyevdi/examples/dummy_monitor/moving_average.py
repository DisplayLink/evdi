import numpy as np

class MovingAverage:
    def __init__(self, depth: int):
        self.depth = depth
        self.values = np.zeros(depth)
        self.pointer = 0
        self.size = 0
        self.current_avg = 0.0

    def push(self, value) -> None:
        """Updates the moving average with the new value"""
        # check if the input is a numpy array
        if isinstance(value, np.ndarray):
            for val in value:
                self._push_single(val)
        else:
            self._push_single(value)

    def _push_single(self, value: float) -> None:
        if self.size < self.depth:
            # We are still filling our initial array
            old = 0
            self.size += 1
        else:
            old = self.values[self.pointer]

        self.values[self.pointer] = value
        self.pointer = (self.pointer + 1) % self.depth
        self.current_avg += (value - old) / self.size

    def average(self) -> float:
        """Returns the current moving average"""
        return self.current_avg
