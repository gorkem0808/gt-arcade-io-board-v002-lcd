# GT ARCADE KONTROL IO BOARD V0.02 LCD

Bu sürümde PC kalibrasyon programı yoktur. Kalibrasyon, buton testi, nişangah testi ve titreşim azaltma ayarı Pico üzerindeki 16x2 I2C LCD menüsünden yapılır.

## Çıkacak UF2 dosyaları

GitHub Actions derlemesinden sonra iki dosya çıkar:

- `gt_arcade_io_p1_lcd_v002.uf2` → 1. oyuncu Pico
- `gt_arcade_io_p2_lcd_v002.uf2` → 2. oyuncu Pico

## Ana özellikler

- 2 oyuncu için 2 Pico
- Her Pico ayrı cihaz adıyla görünür
- P1 cihaz adı: `GT ARCADE IO P1 LCD`
- P2 cihaz adı: `GT ARCADE IO P2 LCD`
- PC programı yok
- CMD yok
- Python yok
- Windows başlangıç programı yok
- 4 köşe kalibrasyon var
- Orta nokta kalibrasyonu yok
- GP20 sistem aktif / pasif
- Titreşimi azalt menüsü LCD üzerinde
- Buton testi LCD üzerinde, GP yazmadan görev isimleriyle

## LCD menü tuşları

- GP19 = Menü aç
- GP2 = Menü yukarı
- GP3 = Menü aşağı
- GP4 = Ayar azalt / sol
- GP5 = Ayar artır / sağ
- GP6 = Seç / Kaydet / Bomba
- GP8 = Geri / İptal / Reload

## LCD menüleri

1. KALIBRASYON
2. BUTON TESTI
3. NISANGAH TEST
4. TITRESIM AZALT
5. AYAR KAYDET
6. CIKIS

## Kalibrasyon

Kalibrasyon sadece 4 köşe ile yapılır:

1. Sol üst
2. Sağ üst
3. Sağ alt
4. Sol alt

Her köşede silahı ilgili yöne çevir ve GP6 tuşuna bas. 4. köşe kaydedilince ayar otomatik hafızaya yazılır.

## Titreşimi azalt

LCD menüsünde üç seviye vardır:

- AZ: hızlı tepki, az filtre
- ORTA: normal kullanım
- YUKSEK: titreme çoksa

GP4 azaltır, GP5 artırır, GP6 kaydeder.

## TeknoParrot seçimi

Player 1:

- Light Gun: `GT ARCADE IO P1 LCD`

Player 2:

- Light Gun: `GT ARCADE IO P2 LCD`

Tetik ve diğer butonlar TeknoParrot'ta cihazdan gelen tuşlara atanır.


## V0.02A GP20 düzeltmesi

GP20 sistem aktif/pasif anahtarıdır. GP20 basılı değilken Pico absolute mouse konumu göndermez; bu sayede normal PC mouse serbest kullanılabilir. GP20 basılıyken potans X/Y nişangahı hareket ettirir ve tetik/bomba aktif olur.
