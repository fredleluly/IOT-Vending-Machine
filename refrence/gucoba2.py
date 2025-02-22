import sys
import cv2
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QLabel, 
                           QVBoxLayout, QHBoxLayout, QPushButton, QGridLayout)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QImage, QPixmap
import random

class WaterButton(QPushButton):
    def __init__(self, icon_path, size_text, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout()
        self.setLayout(layout)
        
        # Icon container
        icon_label = QLabel()
        icon_pixmap = QPixmap(icon_path)
        icon_label.setPixmap(icon_pixmap.scaled(80, 80, Qt.KeepAspectRatio, Qt.SmoothTransformation))
        icon_label.setAlignment(Qt.AlignCenter)
        
        # Size text
        size_label = QLabel(size_text)
        size_label.setAlignment(Qt.AlignCenter)
        
        layout.addWidget(icon_label)
        layout.addWidget(size_label)
        
        self.setStyleSheet("""
            WaterButton {
                background-color: #E6F3FF;
                border-radius: 10px;
                padding: 10px;
                min-height: 120px;
                min-width: 120px;
            }
            WaterButton:hover {
                background-color: #B3D9FF;
            }
        """)

class MachineWidget(QWidget):
    def __init__(self):
        super().__init__()
        layout = QVBoxLayout()
        self.setLayout(layout)
        
        # Progress bar (simulasi)
        progress_widget = QWidget()
        progress_widget.setStyleSheet("""
            background-color: #4F5D75;
            border-radius: 15px;
            padding: 10px;
        """)
        progress_layout = QHBoxLayout(progress_widget)
        
        # Indikator dan progress
        indicator = QLabel("●")
        indicator.setStyleSheet("color: #2ECC71; font-size: 20px;")
        progress = QLabel("▬" * 10)
        progress.setStyleSheet("color: white;")
        progress_layout.addWidget(indicator)
        progress_layout.addWidget(progress)
        
        # Mesin container
        machine_container = QWidget()
        machine_container.setStyleSheet("""
            background-color: #4F5D75;
            border-radius: 15px;
            padding: 20px;
            margin-top: 10px;
        """)
        machine_layout = QVBoxLayout(machine_container)
        
        # Gambar botol (placeholder)
        bottle_label = QLabel()
        bottle_label.setStyleSheet("""
            background-color: #2ECC71;
            border-radius: 10px;
            min-height: 100px;
        """)
        
        machine_layout.addWidget(bottle_label)
        
        layout.addWidget(progress_widget)
        layout.addWidget(machine_container)

class WaterSustainabilityApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Innovative Aqua Solution")
        self.setStyleSheet("background-color: #40E0D0;")
        self.setMinimumSize(1000, 600)
        
        # Widget dan layout utama
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QVBoxLayout(main_widget)
        layout.setContentsMargins(20, 20, 20, 20)
        
        # Judul
        title_label = QLabel("Gambar logo Innovative Aqua Solution")
        title_label.setAlignment(Qt.AlignCenter)
        title_label.setStyleSheet("font-size: 24px; margin-bottom: 20px;")
        layout.addWidget(title_label)
        
        # Layout horizontal untuk konten utama
        content_layout = QHBoxLayout()
        content_layout.setSpacing(20)
        
        # Bagian kiri - Video Display
        self.video_label = QLabel()
        self.video_label.setMinimumSize(640, 360)
        self.video_label.setStyleSheet("background-color: black;")
        content_layout.addWidget(self.video_label)
        
        # Bagian kanan
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setSpacing(20)
        
        # Widget ukuran air
        sizes_widget = QWidget()
        sizes_widget.setStyleSheet("""
            QWidget {
                background-color: white;
                border-radius: 10px;
                padding: 15px;
            }
        """)
        sizes_layout = QVBoxLayout(sizes_widget)
        
        sizes_title = QLabel("Pilihan Ukuran Air")
        sizes_title.setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;")
        sizes_layout.addWidget(sizes_title)
        
        # Grid layout untuk tombol-tombol
        buttons_grid = QGridLayout()
        
        # Data ukuran air dan icon
        water_sizes = [
            ("1.png", "100 ml", 0, 0),
            ("2.png", "350 ml", 0, 1),
            ("3.png", "600 ml", 1, 0),
            ("4.png", "1 Liter", 1, 1)
        ]
        
        for icon_path, size, row, col in water_sizes:
            btn = WaterButton(icon_path, size)
            btn.clicked.connect(lambda checked, s=size: self.on_size_selected(s))
            buttons_grid.addWidget(btn, row, col)
        
        sizes_layout.addLayout(buttons_grid)
        
        # Tambah widget mesin
        self.machine_widget = MachineWidget()
        sizes_layout.addWidget(self.machine_widget)
        
        right_layout.addWidget(sizes_widget)
        
        # Bagian monitoring
        monitoring_widget = QWidget()
        monitoring_layout = QVBoxLayout(monitoring_widget)
        
        monitoring_title = QLabel("Water Quality Monitoring")
        monitoring_title.setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;")
        monitoring_layout.addWidget(monitoring_title)
        
        # Display nilai pH dan TDS
        self.ph_label = QLabel("pH Value: 8.3")
        self.ph_label.setStyleSheet("""
            background-color: white;
            padding: 10px;
            border-radius: 5px;
            font-size: 16px;
            margin: 5px;
        """)
        
        self.tds_label = QLabel("TDS Value: 9 ppm")
        self.tds_label.setStyleSheet("""
            background-color: white;
            padding: 10px;
            border-radius: 5px;
            font-size: 16px;
            margin: 5px;
        """)
        
        monitoring_layout.addWidget(self.ph_label)
        monitoring_layout.addWidget(self.tds_label)
        
        right_layout.addWidget(monitoring_widget)
        content_layout.addWidget(right_panel)
        
        layout.addLayout(content_layout)
        
        # Setup video dan sensor updates
        self.setup_video()
        self.setup_sensor_updates()
    
    def on_size_selected(self, size):
        print(f"Ukuran air dipilih: {size}")
        # Tambahkan logika untuk memproses pemilihan ukuran air
        
    def setup_video(self):
        try:
            self.cap = cv2.VideoCapture('yqq.mp4')
            if not self.cap.isOpened():
                raise Exception("Tidak dapat membuka video")
            
            self.video_timer = QTimer()
            self.video_timer.timeout.connect(self.update_frame)
            self.video_timer.start(33)  # 30 FPS
            
        except Exception as e:
            print(f"Error: {str(e)}")
            self.video_label.setText("")
    
    def update_frame(self):
        try:
            ret, frame = self.cap.read()
            if ret:
                if frame is None:
                    self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                    ret, frame = self.cap.read()
                
                rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                h, w, ch = rgb_frame.shape
                bytes_per_line = ch * w
                qt_image = QImage(rgb_frame.data, w, h, bytes_per_line, QImage.Format_RGB888)
                scaled_pixmap = QPixmap.fromImage(qt_image).scaled(
                    self.video_label.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation)
                self.video_label.setPixmap(scaled_pixmap)
            else:
                self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
        except Exception as e:
            print(f"Error update frame: {str(e)}")
    
    def setup_sensor_updates(self):
        self.sensor_timer = QTimer()
        self.sensor_timer.timeout.connect(self.update_sensor_values)
        self.sensor_timer.start(2000)
    
    def update_sensor_values(self):
        ph = round(random.uniform(8.0, 8.4), 1)
        self.ph_label.setText(f"pH Value: {ph}")
        
        tds = random.randint(6, 10)
        self.tds_label.setText(f"TDS Value: {tds} ppm")
    
    def closeEvent(self, event):
        if hasattr(self, 'cap'):
            self.cap.release()
        super().closeEvent(event)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = WaterSustainabilityApp()
    window.show()
    sys.exit(app.exec_())