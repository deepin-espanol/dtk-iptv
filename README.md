# IPTV

Reproductor de IPTV nativo para deepin, escrito en C++ con Qt 6 y DTK 6.

- Usa por defecto el catálogo de [iptv-org](https://github.com/iptv-org/iptv/blob/master/PLAYLISTS.md)
  (categorías, idiomas, países y regiones). La app no incluye ninguna lista: descarga
  `PLAYLISTS.md` de GitHub en el primer arranque, lo guarda en caché y lo actualiza en cada inicio.
- Permite agregar listas M3U/M3U8 propias (URL o archivo local) desde **Agregar lista**.
- La decodificación es por software por defecto; la de hardware se activa en el menú.
- Cambio de canal como en una tele: botones anterior/siguiente o las teclas Av Pág / Re Pág.
- Pantalla al sintonizar configurable en Ajustes: estática, "sin señal" o un color liso.
- Escáner de canales: comprueba cuáles funcionan en la lista abierta y permite ocultar los que no.
- Todo se gestiona desde **Ajustes** (en el menú): pantalla de sintonización, decodificación,
  actualización del catálogo, listas propias y escaneo.
- El panel de listas se puede ocultar (Ctrl+B).
- Favoritos, búsqueda de canales, filtro por grupo y pantalla completa (F11 o doble clic).

## Sobre los canales

Esta aplicación es **solo un reproductor**. No aloja, emite ni distribuye ningún canal ni
contenido de vídeo, y no incluye ninguna emisión ni lista de canales: el catálogo se descarga
de iptv-org cuando se usa la aplicación.

Los canales y listas que aparecen por defecto provienen del proyecto
[iptv-org/iptv](https://github.com/iptv-org/iptv) y obedecen a su
[licencia](https://github.com/iptv-org/iptv/blob/master/LICENSE) y a sus condiciones de uso.
Son enlaces a emisiones públicas recopilados por esa comunidad; su contenido, disponibilidad
y derechos corresponden a cada emisora. Cualquier reclamación sobre un canal debe dirigirse
a iptv-org o a la emisora correspondiente.

Las listas que agregue el usuario son responsabilidad suya.

## Compilar

Dependencias: Qt 6 (Widgets, Network, Multimedia con backend FFmpeg) y DTK 6 (`libdtk6widget-dev`).
En ejecución se recomienda tener `ffmpeg` instalado (incluye `ffprobe`): la app lo usa para conectar
con los canales (cabeceras `User-Agent`/`Referer`, reintentos y mensajes de error claros) y para el
escáner. Sin él reproduce directamente y el escáner queda desactivado.

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/dtk-iptv
```

## Paquete .deb

El directorio [debian/](debian/) contiene todo lo necesario para generar el paquete:

```sh
sudo apt install build-essential debhelper cmake ninja-build qt6-base-dev qt6-multimedia-dev \
    libdtk6core-dev libdtk6gui-dev libdtk6widget-dev
dpkg-buildpackage -us -uc -b
sudo apt install ../dtk-iptv_1.0.0_amd64.deb
```

El `.deb` se genera en el directorio superior. Para publicar una versión nueva, cambia la versión
en `CMakeLists.txt` y añade una entrada en `debian/changelog`.
