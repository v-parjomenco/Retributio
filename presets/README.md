# SkyGuard Stress Presets

## Файлы

| Пресет | Назначение | Примерный масштаб |
|--------|-----------|-------------------|
| `active_small.env` | Smoke: корректность wiring, метрики читаемы | ~1K entities (8 × 4×4 chunks) |
| `active_large.env` | Soak: spatial index capacity, streaming, memory ceiling | ~1M+ entities (64 × 128×128 chunks) |

## ENV-ключи

| Ключ | Consumer | Описание |
|------|----------|----------|
| `SKYGUARD_STRESS_ENABLED` | Config builder + Provider | Включить stress mode (1/0) |
| `SKYGUARD_STRESS_SEED` | Provider | Seed детерминированного генератора |
| `SKYGUARD_STRESS_ENTITIES_PER_CHUNK` | Config builder + Provider | Количество сущностей на чанк |
| `SKYGUARD_STRESS_TEXTURE_COUNT` | Provider | Количество текстур для ротации |
| `SKYGUARD_STRESS_Z_LAYERS` | Provider | Количество z-уровней (sort pressure) |
| `SKYGUARD_STRESS_WINDOW_WIDTH` | Config builder | Ширина окна стриминга в чанках |
| `SKYGUARD_STRESS_WINDOW_HEIGHT` | Config builder | Высота окна стриминга в чанках |
| `SKYGUARD_STRESS_TEXTURE_IDS` | Provider | CSV индексов текстур (опционально, вместо COUNT) |

## Запуск

### Visual Studio 2022+ (рекомендуемый)

Project → Properties → Debugging → Environment. Вставить содержимое .env файла (без строк-комментариев):

```
SKYGUARD_STRESS_ENABLED=1
SKYGUARD_STRESS_SEED=42
SKYGUARD_STRESS_ENTITIES_PER_CHUNK=64
SKYGUARD_STRESS_TEXTURE_COUNT=3
SKYGUARD_STRESS_Z_LAYERS=5
SKYGUARD_STRESS_WINDOW_WIDTH=128
SKYGUARD_STRESS_WINDOW_HEIGHT=128
```

### PowerShell

```powershell
Get-Content .\presets\active_large.env |
  Where-Object { $_ -and -not $_.StartsWith('#') } |
  ForEach-Object {
    $kv = $_.Split('=', 2)
    Set-Item -Path "Env:$($kv[0])" -Value $kv[1]
  }

# Запуск из того же shell:
.\out\build\Profile\SFML1.exe
```

### cmd.exe

```cmd
for /f "usebackq eol=# tokens=*" %%L in ("presets\active_large.env") do set "%%L"
SFML1.exe
```

Для интерактивного использования (без .bat):
```cmd
for /f "usebackq eol=# tokens=*" %L in ("presets\active_large.env") do set "%L"
```

## KPI

- **active_small:** wiring корректен, overlay метрики читаемы, запуск < 2 сек.
- **active_large:** active-set ceiling стабилен, streaming без утечек, spatial registration без overflow.