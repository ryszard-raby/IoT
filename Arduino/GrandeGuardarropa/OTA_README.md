# OTA (Over The Air) Update Instructions

## Opis
Projekt został wyposażony w funkcjonalność OTA (Over The Air), która pozwala na aktualizację oprogramowania mikrokontrolera bez fizycznego dostępu do urządzenia.

## Konfiguracja
- **Hostname**: GrandeGuardarropa
- **Port**: 8266
- **Hasło**: update123

## Jak przeprowadzić aktualizację OTA

### Metoda 1: Arduino IDE
1. Otwórz Arduino IDE
2. Upewnij się, że komputer i ESP8266 są w tej samej sieci WiFi
3. W menu Tools > Port wybierz opcję sieciową: `GrandeGuardarropa at [IP_ADDRESS]`
4. Kliknij Upload jak zwykle
5. Gdy zostaniesz poproszony o hasło, wprowadź: `update123`

### Metoda 2: PlatformIO
1. **Dla standardowych aktualizacji OTA (zalecane)**:
   ```bash
   pio run -e GrandeGuardarropa -t upload
   ```

2. **Dla pierwszego upload'u przez kabel USB**:
   ```bash
   pio run -e GrandeGuardarropa_USB -t upload
   ```

3. **Jeśli hostname nie działa, użyj adresu IP**:
   - Sprawdź adres IP w Serial Monitor
   - Zaktualizuj `upload_port` w środowisku `GrandeGuardarropa_IP`
   - Uruchom:
   ```bash
   pio run -e GrandeGuardarropa_IP -t upload
   ```

4. **W VS Code z PlatformIO**:
   - Wybierz odpowiednie środowisko w PlatformIO Project Tasks
   - Kliknij "Upload" dla wybranego środowiska

### Metoda 3: Programowo (curl/HTTP)
```bash
curl -F "image=@.pio/build/GrandeGuardarropa/firmware.bin" [IP_ADDRESS]:8266/update
```

## Sprawdzanie adresu IP
Adres IP urządzenia jest wyświetlany w monitorze szeregowym po uruchomieniu:
```
OTA Ready
IP address: 192.168.1.XXX
Hostname: GrandeGuardarropa
OTA Password: update123
```

## Bezpieczeństwo
- Zmień domyślne hasło OTA w kodzie przed wdrożeniem produkcyjnym
- Upewnij się, że sieć WiFi jest bezpieczna
- OTA działa tylko w tej samej sieci lokalnej

## Rozwiązywanie problemów
1. **Urządzenie nie pojawia się w liście portów**:
   - Sprawdź czy ESP8266 i komputer są w tej samej sieci
   - Zrestartuj urządzenie
   - Sprawdź firewall

2. **Błąd autoryzacji**:
   - Sprawdź czy hasło OTA jest poprawne
   - Upewnij się, że używasz najnowszej wersji Arduino IDE/PlatformIO

3. **Aktualizacja się nie powiodła**:
   - Sprawdź czy jest wystarczająco miejsca na flash
   - Sprawdź stabilność połączenia WiFi
   - Sprawdź logi w monitorze szeregowym

## Dostosowanie
Możesz zmienić konfigurację OTA w pliku `main.cpp`:
```cpp
OTAService otaService("NowyHostname", "NoweHasło");
```

## Konfiguracja PlatformIO
Projekt zawiera trzy środowiska w `platformio.ini`:

1. **GrandeGuardarropa** (domyślne) - aktualizacje OTA przez hostname
2. **GrandeGuardarropa_USB** - pierwszy upload przez kabel USB
3. **GrandeGuardarropa_IP** - aktualizacje OTA przez adres IP

### Zmiana hasła OTA w platformio.ini
Jeśli zmieniasz hasło w kodzie, zaktualizuj także `upload_flags` w `platformio.ini`:
```ini
upload_flags = 
	--auth=NoweHasło
	--port=8266
```

### Zmiana adresu IP
Dla środowiska `GrandeGuardarropa_IP`, zaktualizuj `upload_port`:
```ini
upload_port = 192.168.1.XXX
```
