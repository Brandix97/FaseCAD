// Plugin de ejemplo de FaseCAD: lista de aparatos.
// Cada comando de plugin.json llama a una función de este archivo.
// Todo lo que se puede hacer está en el objeto global "qet": las mismas
// llamadas que los scripts, más qet.dialog(), qet.chooseSaveFile() y
// qet.writeTextFile() para los plugins.

// Los aparatos de todo el proyecto, uno por fila.
function aparatos() {
    var filas = [];
    for (var f = 0; f < qet.folioCount(); f++) {
        var uuids = qet.elementUuids(f);
        for (var i = 0; i < uuids.length; i++) {
            filas.push({
                folio: f + 1,
                marca: qet.elementLabel(f, uuids[i]),
                designacion: qet.elementInfo(f, uuids[i], "designation"),
                fabricante: qet.elementInfo(f, uuids[i], "manufacturer"),
                referencia: qet.elementInfo(f, uuids[i], "manufacturer_reference")
            });
        }
    }
    return filas;
}

function csv(valor, separador) {
    valor = String(valor === undefined ? "" : valor);
    if (valor.indexOf(separador) >= 0 || valor.indexOf('"') >= 0)
        valor = '"' + valor.replace(/"/g, '""') + '"';
    return valor;
}

function exportarAparatos() {
    var opciones = qet.dialog({
        title: "Exportar aparatos",
        text: "Escribe un CSV con los aparatos de " + qet.projectTitle() + ".",
        ok: "Exportar",
        fields: [
            {id: "separador", label: "Separador", type: "choice", items: [";", ",", "Tabulador"], value: ";"},
            {id: "sinMarca", label: "Incluir aparatos sin marca", type: "check", value: false}
        ]
    });
    if (!opciones) return;  // cancelado

    var sep = opciones.separador === "Tabulador" ? "\t" : opciones.separador;
    var ruta = qet.chooseSaveFile("Guardar la lista de aparatos", "CSV (*.csv)", "aparatos.csv");
    if (!ruta) return;

    var lineas = [["Folio", "Marca", "Designación", "Fabricante", "Referencia"].join(sep)];
    var filas = aparatos();
    var escritas = 0;
    for (var i = 0; i < filas.length; i++) {
        var a = filas[i];
        if (!a.marca && !opciones.sinMarca) continue;
        lineas.push([a.folio, a.marca, a.designacion, a.fabricante, a.referencia]
                    .map(function (v) { return csv(v, sep); }).join(sep));
        escritas++;
    }
    if (qet.writeTextFile(ruta, lineas.join("\r\n") + "\r\n"))
        qet.showMessage(escritas + " aparatos escritos en " + ruta);
}

function resumen() {
    var filas = [];
    for (var f = 0; f < qet.folioCount(); f++) {
        filas.push([String(f + 1), qet.folioTitle(f),
                    String(qet.elementCount(f)), String(qet.conductorCount(f))]);
    }
    qet.dialog({
        title: "Resumen del proyecto",
        text: qet.projectTitle() + ": " + qet.folioCount() + " folios.",
        fields: [
            {id: "folios", label: "Folios", type: "table", readOnly: true,
             columns: ["Folio", "Título", "Aparatos", "Cables"], value: filas}
        ]
    });
}
