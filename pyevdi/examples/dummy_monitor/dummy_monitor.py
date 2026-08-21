import signal
import time
import PyEvdi
import argparse
import os
import sys
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QImage, QPainter, QColor, QPixmap
from PySide6.QtWidgets import QApplication, QMainWindow, QWidget
import numpy as np

from moving_average import MovingAverage

def is_not_running_as_root():
    return os.geteuid() != 0

def get_available_evdi_card():
    for i in range(20):
        if PyEvdi.check_device(i) == PyEvdi.AVAILABLE:
            return i
    PyEvdi.add_device()
    for i in range(20):
        if PyEvdi.check_device(i) == PyEvdi.AVAILABLE:
            return i
    return -1

def load_edid_file(file):
  if os.path.exists(file):
    with open(file, mode='rb') as f:
      ed = f.read()
    return ed
  elif os.path.exists(file + '.edid'):
    with open(file + '.edid', mode='rb') as f:
      ed = f.read()
    return ed
  else:
    return None

class Options:
  headless: bool = False
  resolution: tuple[int, int] = (1920, 1080)
  refresh_rate: int = 60
  edid_file: str = None
  fps_limit: int = 60

class ImageBufferWidget(QWidget):
    def __init__(self, width, height, options: Options):
        super().__init__()
        self.setMinimumSize(width, height)
        self.options = options
        width, height = options.resolution
        self.pixmap = QPixmap(self.width(), self.height())
        self.pixmap.fill(QColor(255, 0, 0))
        self.painter = QPainter(self)

    def paintEvent(self, event):
      print("paintEvent")

      self.painter.begin(self)

      new_pixmap = self.scale_image(self.pixmap)

      # Calculate the starting x and y coordinates to center the image
      start_x = (self.width() - new_pixmap.width()) // 2
      start_y = (self.height() - new_pixmap.height()) // 2

      self.painter.drawPixmap(start_x, start_y, new_pixmap)

      self.painter.end()

    def scale_image(self, image: QImage) -> QImage:
      scale_width = self.width() / image.width()
      scale_height = self.height() / image.height()
      scale = min(scale_width, scale_height)

      scaled_width = int(image.width() * scale)
      scaled_height = int(image.height() * scale)

      scaled_width = min(scaled_width, self.width())
      scaled_height = min(scaled_height, self.height())

      return image.scaled(scaled_width, scaled_height, Qt.KeepAspectRatio)

    def update_image(self, buffer):
        print("update_image: buffer id:", buffer.id)
        now = time.time()
        # x_size, y_size = buffer.width, buffer.height
        # for y in range(y_size):
        #     for x in range(x_size):
        #         rgb: int = buffer.bytes[y, x]
        #         bytes = [rgb >> 24, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF]
        #         color =  QColor(bytes[1], bytes[2], bytes[3])
        #         self.image.setPixelColor(x, y, color)

        # np_array = np.array(buffer, copy = False) # This is possible thanks to buffer protocol
        # self.image = QImage(np_array, buffer.width, buffer.height, QImage.Format_RGB32)

        image = QImage(buffer.bytes, buffer.width, buffer.height, QImage.Format_RGB32)
        self.pixmap = self.pixmap.fromImage(image)

        took = time.time() - now
        print("update_image: took", took, "seconds")
        self.repaint()

class MainWindow(QMainWindow):
    def __init__(self, options: Options):
        super().__init__()
        self.setWindowTitle("EVDI virtual monitor")
        self.image_buffer_widget = ImageBufferWidget(128, 128, options)
        self.setCentralWidget(self.image_buffer_widget)

    def resizeEvent(self, event):
        self.image_buffer_widget.resize(event.size())

last_frame_time = 0
fps_move_average = MovingAverage(10)

def format_buffer(buffer):
    result = []
    result.append(f"received buffer id: {buffer.id}")
    result.append(f"rect_count: {buffer.rect_count}")
    result.append(f"width: {buffer.width}")
    result.append(f"height: {buffer.height}")
    result.append(f"stride: {buffer.stride}")
    result.append("rects:")
    for rect in buffer.rects:
        result.append(f"{rect.x1}, {rect.y1}, {rect.x2}, {rect.y2}")
    return "\n".join(result)

def framebuffer_handler(buffer, app):
  global last_frame_time

  print(format_buffer(buffer))

  now = time.time()
  time_since_last_frame = now - last_frame_time

  fps = 1000 / time_since_last_frame
  fps_move_average.push(fps)

  if app is not None:
    app.image_buffer_widget.update_image(buffer)
  del buffer
  last_frame_time = time.time()

def mode_changed_handler(mode, app) -> None:
    print(format_mode(mode))

def format_mode(mode) -> None:
    return 'Mode: ' + str(mode.width) + 'x' + str(mode.height) + '@' + str(mode.refresh_rate) + ' ' + str(mode.bits_per_pixel) + 'bpp ' + str(mode.pixel_format)

def main(options: Options) -> None:
    card = PyEvdi.Card(get_available_evdi_card())
    area = options.resolution[0] * options.resolution[1]
    connect_ret = None
    if options.edid_file:
      edid = load_edid_file(options.edid_file)
      connect_ret = card.connect(edid, len(edid), area, area * options.refresh_rate)
    else:
      connect_ret = card.connect(None, 0, area, area * options.refresh_rate)


    my_app = None
    card.acquire_framebuffer_handler = lambda buffer: framebuffer_handler(buffer, my_app)
    card.mode_changed_handler = lambda mode: mode_changed_handler(mode, my_app)
    mode = card.getMode()

    if not options.headless:
      print("RET:", connect_ret)
      app = QApplication([])
      my_app = MainWindow(options)
      my_app.show()
      # set window size to 480x270
      my_app.resize(480, 270)

      card_timer = QTimer()
      card_timer.timeout.connect(lambda: card.handle_events(0))
      card_timer.setInterval(20)
      card_timer.start()

      signal.signal(signal.SIGINT, lambda *args: app.quit())

      def on_app_quit():
        now = time.time()
        print("Quitting at", now)
        card.disconnect()
        card.close()
        took = time.time() - now
        print("Took", took, "seconds")
      app.aboutToQuit.connect(on_app_quit)

      print("Starting event loop")

      sys.exit(app.exec())
    else:

      print("Running headless")
      while(True):
        now = time.time()
        #print("Handling events at", now)
        card.handle_events(100)
        took = time.time() - now
        #print("Took", took, "seconds")

      card.disconnect()
      card.close()

if __name__ == '__main__':
  # read arguments into options
  options = Options()

  # parse arguments
  parser = argparse.ArgumentParser()
  parser.add_argument('--headless', action='store_true')
  parser.add_argument('--resolution', nargs=2, type=int)
  parser.add_argument('--refresh-rate', type=int)
  parser.add_argument('--edid-file', type=str)
  parser.add_argument('--fps-limit', type=int)
  args = parser.parse_args()

  # set options
  if args.headless:
    options.headless = args.headless
  if args.edid_file:
    options.edid_file = args.edid_file
  if args.resolution:
    options.resolution = args.resolution
  if args.refresh_rate:
    options.refresh_rate = args.refresh_rate
  if args.fps_limit:
    options.fps_limit = args.fps_limit

  main(options)