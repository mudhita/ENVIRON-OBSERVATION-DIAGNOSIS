Implementasi komunikasi data antar ESP32 menggunakan jaringan mesh dan koneksi serial, kami berhasil mengintegrasikan data dari sensor yang tersebar di beberapa lokasi ke satu node pusat dan mengirimkannya ke server

1.	Alat dan Bahan
a.	Alat:
•	ESP32 Development Boards (7 buah)
•	Kabel Jumper
•	Komputer/Laptop untuk pengembangan dan debugging
•	Akses Internet
b.	Bahan:
•	Arduino IDE
•	Library PainlessMesh untuk ESP32
•	Library PubSubClient untuk ESP32
•	Library WiFi untuk ESP32
2.	Alur Kerja
a.	Persiapan Perangkat:
•	Siapkan tujuh ESP32 development boards.
•	Hubungkan ESP32 #1 hingga #5 dengan sensor untuk mengukur humidity, temperature, gas concentration, dan luminosity.
•	Pastikan semua ESP32 terhubung ke sumber daya listrik
•	Hubungkan ESP32 #6 dan  #7 dengan menggunakan serial uart
•	Hubungkan ESP32 #7 dengan sambungan jaringan Internet
b.	Perangkat ESP32 #1 hingga #5:
•	Setiap ESP32 mengumpulkan data dari sensor yang terhubung.
•	Data yang dikumpulkan diformat menjadi string.
•	Data dikirimkan melalui jaringan mesh ke ESP32 #6.
c.	Perangkat ESP32 #6:
•	ESP32 #6 bertindak sebagai node data center yang menerima data dari ESP32 #1 hingga #5 melalui jaringan mesh.
•	Data yang diterima diproses, kemudian diteruskan melalui koneksi serial ke ESP32 #7.
d.	Perangkat ESP32 #7:
•	ESP32 #7 menerima data dari ESP32 #6 melalui koneksi serial uart.
•	Data yang diterima dipublikasikan ke broker MQTT dan Influxdb menggunakan koneksi WiFi.
•	ESP32 #7 memiliki fungsi tambahan yakni OTA-drive
