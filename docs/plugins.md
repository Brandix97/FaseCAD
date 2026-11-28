# Plugins de FaseCAD

Un plugin añade comandos propios a FaseCAD: botones en las pestañas, ventanas
para pedir datos, exportaciones e informes hechos a medida de un cliente. Se
escribe en JavaScript con la misma API `qet` que los scripts, así que funciona
en cualquier PC con FaseCAD sin compilar nada, y sigue funcionando en las
versiones siguientes mientras pida una versión de la API que FaseCAD tenga.

## Qué es un plugin

Una carpeta con:

- `plugin.json`: quién lo hace, qué versión de la API necesita y qué comandos añade.
- uno o varios `.js`: cada comando llama a una función de uno de ellos.
- sus iconos (`.svg` o `.png`), si los tiene.

Para repartirlo, se comprime esa carpeta en un zip y se le cambia la extensión a
`.fasplugin`. El `plugin.json` puede ir en la raíz del zip o dentro de una sola
carpeta.

## plugin.json

```json
{
  "id": "acme.exportar-erp",
  "name": "Exportar al ERP",
  "version": "1.0.0",
  "author": "Acme",
  "description": "Escribe los artículos del proyecto para el ERP.",
  "api": 1,
  "permissions": ["files"],
  "tab": "Acme",
  "commands": [
    {
      "id": "exportar",
      "label": "Exportar\nal ERP",
      "tooltip": "Escribe el XML que lee el ERP",
      "icon": "erp.svg",
      "group": "ERP",
      "size": "large",
      "script": "main.js",
      "function": "exportarErp"
    }
  ]
}
```

| Clave | Qué es |
|---|---|
| `id` | Único: letras, cifras, `.`, `-` y `_`. Es también el nombre de su carpeta. |
| `name`, `version`, `author`, `description` | Lo que enseña el gestor de plugins. |
| `api` | La versión de la API que necesita. Esta versión de FaseCAD da la 1. Un plugin que pide una más nueva no se carga y lo dice. |
| `permissions` | Lo que puede hacer además de trabajar sobre el proyecto abierto. Hoy: `files`, escribir archivos. Se enseña antes de instalar. |
| `tab` | La pestaña donde van sus botones. Sin ella van a la pestaña **Plugins**. |
| `commands` | Sus comandos. `label` admite `\n` para partir el texto del botón; `size` es `large` o pequeño; `group` es el grupo dentro de la pestaña (por defecto, el nombre del plugin). |

Cada comando sale también en **Proyecto › Plugins**, en el buscador de comandos
y en **Configuración › Atajos**, donde se le puede dar una tecla.

## El script

```js
function exportarErp() {
    var opciones = qet.dialog({
        title: "Exportar al ERP",
        ok: "Exportar",
        fields: [
            {id: "pedido", label: "Pedido", type: "text", value: "PO-1"},
            {id: "repuestos", label: "Con repuestos", type: "check", value: true}
        ]
    });
    if (!opciones) return;                 // el usuario canceló
    var ruta = qet.chooseSaveFile("Guardar", "XML (*.xml)", opciones.pedido + ".xml");
    if (!ruta) return;
    var xml = "<pedido id=\"" + opciones.pedido + "\">\n";
    for (var f = 0; f < qet.folioCount(); f++) {
        var uuids = qet.elementUuids(f);
        for (var i = 0; i < uuids.length; i++)
            xml += "  <aparato marca=\"" + qet.elementLabel(f, uuids[i]) + "\"/>\n";
    }
    qet.writeTextFile(ruta, xml + "</pedido>\n");
}
```

Lo que un comando cambia en el proyecto se deshace de una vez con Ctrl+Z
(«Plugin: Exportar al ERP»). El tiempo que una ventana espera al usuario no
cuenta para el límite de tiempo de un script.

### Lo nuevo para plugins

| Llamada | Qué hace |
|---|---|
| `qet.dialog(spec)` | Una ventana con campos. Devuelve los valores por `id`, o `null` si se cancela. |
| `qet.chooseSaveFile(título, filtro, nombre)` | Dónde guardar un archivo. Vacío si se cancela. |
| `qet.writeTextFile(ruta, texto)` | Escribe un archivo de texto en UTF-8. |
| `qet.pluginFolder()` | La carpeta del plugin. |
| `qet.fasecadVersion()`, `qet.apiVersion()` | La versión de FaseCAD y de la API. |

Tipos de campo de `qet.dialog`: `text`, `multiline`, `number` (`min`, `max`,
`suffix`), `decimal` (`decimals`), `choice` (`items`, `editable`), `check` y
`table` (`columns`, `value` como lista de filas, `readOnly`). Todos admiten
`label`, `value` y `tooltip`.

El resto de la API (leer y cambiar aparatos, cables, bornes, folios, exportar a
PDF, DXF, listas...) es la de los scripts; la lista entera la da
`qet.apiSignatures()`.

## Instalar, apagar y quitar

**Herramientas › Plugins** (o **Proyecto › Plugins › Plugins…**):

- **Instalar...** un `.fasplugin`: antes enseña quién lo hace y qué puede hacer.
- La casilla de cada plugin lo enciende o lo apaga sin quitarlo.
- **Quitar** lo borra.
- **Abrir la carpeta de plugins**: para trabajar en uno a mano. Tras cambiarlo,
  **Recargar**.

Instala solo plugins de alguien de confianza: un plugin hace lo que su script
dice sobre el proyecto abierto.

## Ejemplo

`examples/plugins/fasecad.ejemplo-aparatos` exporta los aparatos a CSV con una
ventana de opciones y enseña un resumen por folio. Es un buen punto de partida.
