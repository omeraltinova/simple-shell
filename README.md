# Modüler Terminal Projesi

## Genel Bakış

**Modüler Terminal**, GTK4 tabanlı çoklu sekme destekli bir terminal emülatörüdür. Bu proje, sistem programlama konseptlerini (process yönetimi, IPC), grafiksel arayüz geliştirmeyi ve modüler tasarımı (MVC mimarisi) bir araya getirir.

![Modüler Terminal](https://placeholder.com/modüler-terminal-ekran-görüntüsü.png)

## Özellikler

- **Çoklu Sekme Desteği**: Birden fazla terminal sekmesi açıp yönetme
- **Komut Yürütme**: Standart shell komutlarını çalıştırma (`ls`, `grep`, vb.)
- **Süreç Yönetimi**: Çalışan processleri izleme ve kontrol etme
- **Komut Geçmişi**: Yukarı/aşağı ok tuşları ile geçmiş komutlara erişim
- **Renkli Çıktı**: Farklı çıktı türleri için renk kodlaması
- **Mesajlaşma**: Sekmeler arası paylaşılan bellek üzerinden iletişim

## Mimari

Proje, **MVC (Model-View-Controller)** mimarisini uygulamaktadır:

### 1. Model
- Process yönetimi  
- Paylaşılan bellek iletişimi  
- Komut geçmişi  

### 2. View
- GTK4 terminal arayüzü  
- Sekme yönetimi  
- Çıktı görüntüleme  

### 3. Controller
- Komut/mesaj ayrımı  
- Özel komutların yönetimi  
- Model ve View arasındaki iletişim  

## Kurulum

### Gereksinimler

- C derleyicisi (GCC önerilir)
- GTK 4.0 veya üzeri
- POSIX uyumlu işletim sistemi (Linux, macOS)

### Bağımlılıkları Kurma

#### Ubuntu/Debian
```bash
sudo apt install build-essential libgtk-4-dev
```

#### Fedora
```bash
sudo dnf install gcc gtk4-devel
```

#### Arch Linux
```bash
sudo pacman -S gcc gtk4
```

### Derleme
```bash
make
```

### Çalıştırma
```bash
make run
```
veya
```bash
./terminal_app
```

## Kullanım

### Temel Komutlar

- Standart Unix/Linux komutları (`ls`, `cat`, `grep` vb.)
- Mesaj gönderme: `@msg <mesaj>`  
- İçe gömülü komutlar:
  - `clear`: Terminal ekranını temizler
  - `help`: Komut listesini gösterir
  - `version`: Sürüm bilgisini gösterir
  - `date`: Sistem tarihini gösterir
  - `whoami`: Mevcut kullanıcı adını gösterir
  - `uptime`: Sistem çalışma süresini gösterir
  - `joke`: Rastgele bir programlama şakası gösterir
  - `ps`: Çalışan süreçleri listeler

### Arayüz Kullanımı

- **Yeni Sekme**: Sağ üstteki "+" butonuna tıklayın  
- **Sekmeyi Kapatma**: Sekme başlığındaki "X" butonuna tıklayın  
- **Sekme Sıralaması**: Sekmeleri sürükle-bırak ile yeniden düzenleyin  
- **Komut Girişi**: Alt kısımdaki metin kutusuna komutları yazın  
- **Komut Geçmişi**: Yukarı/aşağı ok tuşları ile önceki komutları görüntüleyin  
- **En Alta Kaydırma**: "↓" butonuna tıklayarak çıktı penceresinin en altına gidin  

## Makefile Açıklaması

Projede kullanılan Makefile, derlenme sürecini otomatikleştirir:

```makefile
CC=gcc
CFLAGS=`pkg-config --cflags gtk4` -Wall -g
LDFLAGS=`pkg-config --libs gtk4`

SRCS=main.c controller.c view.c model.c
OBJS=$(SRCS:.c=.o)
TARGET=terminal_app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o $(TARGET)

run: all
	./$(TARGET)
```

### Kullanım:
- `make`: Projeyi derler  
- `make run`: Projeyi derleyip çalıştırır  
- `make clean`: Derleme ürünlerini temizler  

## Proje Yapısı

```
terminal-project/
│
├── main.c         # Ana giriş noktası
├── model.c        # Veri ve iş mantığı
├── model.h        # Model API tanımları
├── view.c         # Arayüz ve görüntüleme
├── view.h         # View API tanımları
├── controller.c   # Kullanıcı girdisi ve kontrol mantığı
├── controller.h   # Controller API tanımları
├── Makefile       # Derleme kuralları
└── README.md      # Bu belge
```

## İletişim ve Mesajlaşma

Terminal sekmeleri arasında **paylaşılan bellek (shared memory)** kullanarak mesaj alışverişi yapabilirsiniz.  
Bir sekmeden diğerine mesaj göndermek için:

```
@msg Merhaba, bu bir test mesajıdır!
```

Mesajlar tüm sekmelerde **mavi renkle** gösterilir ve **gönderici sekme numarası** ile etiketlenir.

## Katkıda Bulunma

1. Projeyi forklayın  
2. Feature branch'i oluşturun  
   ```bash
   git checkout -b feature/amazing-feature
   ```
3. Değişikliklerinizi commit edin  
   ```bash
   git commit -m "Add some amazing feature"
   ```
4. Branch'inizi push edin  
   ```bash
   git push origin feature/amazing-feature
   ```
5. Pull request açın  

## Lisans

Bu proje MIT lisansı altında lisanslanmıştır.

---

**Bu proje BIL244 Sistem Programlama dersi için oluşturulmuştur.**
