# Smart Door Arduino Code

Kode Arduino untuk Smart Door berbasis IoT, skripsi Reza (Polinema).

## 🔷 Deskripsi
Project ini merupakan bagian dari penelitian skripsi dengan judul **“Integrasi Voice Recognition dan Knock Code pada Sistem Smart Door Berbasis IoT untuk Keamanan yang Inklusif.”**

Sistem ini mengintegrasikan:
- **Voice command** melalui Firebase
- **Knock code detection** dengan sensor piezoelectric
- **Push button** untuk membuka pintu manual dari dalam ruangan
- **Relay control** untuk mengaktifkan solenoid lock

## 🔷 Hardware yang digunakan
- ESP32 Dev Module
- Sensor Piezoelectric
- Sensor Magnetik MC-38
- Push Button
- Relay + Solenoid Lock
- LED indikator

## 🔷 Library yang diperlukan
- WiFi.h
- Firebase_ESP_Client.h

Pastikan library di atas sudah terinstall di Arduino IDE.

## 🔷 Cara upload program
1. Buka file `.ino` di Arduino IDE.
2. Pilih board **ESP32 Dev Module** dan port COM sesuai ESP32 kamu.
3. Klik **Upload** untuk memprogram ke board.

## 🔷 Author
Dibuat oleh **Reza (Polinema)** untuk tugas akhir skripsi 2025.

## 🔷 Lisensi
Project ini bersifat open-source untuk kebutuhan pembelajaran dan penelitian.

