#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Make the example templates FaseCAD ships with.

Step 1 (this script, "project"): write a QElectroTech project with one
sheet per template, in the file format every version reads (wires join
terminals by their number), using the symbols of the common collection.

Step 2 (the program): open and save it again so it is written in the
current format, and check it by exporting the sheets to PNG:

    qelectrotech --resave examples-src.qet examples.qet
    qelectrotech --export-png examples.qet png/

Step 3 (this script, "templates"): turn each sheet of the saved project
into a template (.qetmak), the format "Create a template" writes: the
definitions of the symbols used, then the sheet's own XML.

    python make_example_templates.py project <elements dir> examples-src.qet
    python make_example_templates.py templates <elements dir> examples.qet <out dir>
"""

import sys
import uuid
import xml.etree.ElementTree as ET

ORIENT = {"n": "0", "e": "1", "s": "2", "w": "3"}

GV = "10_electric/10_allpole/200_fuses_protective_gears/12_magneto_thermal_circuit_breakers/gv2p.elmt"
KM = "10_electric/10_allpole/310_relays_contactors_contacts/02_contacts_cross_referencing/02_power_contacts/com_puiss4.elmt"
M3 = "10_electric/10_allpole/391_consumers_actuators/10_engines/moteur_tri.elmt"
M6 = "10_electric/10_allpole/391_consumers_actuators/10_engines/moteur_tri_de.elmt"
DI = "10_electric/10_allpole/395_electronics_semiconductors/41_PLC_controllers/cart_univ_entr.elmt"
DO = "10_electric/10_allpole/395_electronics_semiconductors/41_PLC_controllers/cart_univ_sort.elmt"
PB = "10_electric/10_allpole/380_signaling_operating/20_push_buttons/poussoir.elmt"
IND = "10_electric/10_allpole/390_sensors_instruments/02_sensors_inductive/capteur_ind_no_3p.elmt"
LAMP = "10_electric/10_allpole/380_signaling_operating/11_optical_signaling/lampe2.elmt"
COIL = "10_electric/10_allpole/310_relays_contactors_contacts/01_coils/bobine3.elmt"
PRESS = "10_electric/10_allpole/390_sensors_instruments/13_sensors_pressure/capteur_pression.elmt"
TEMP = "10_electric/10_allpole/390_sensors_instruments/15_sensors_temperature/sonde_temperature.elmt"


def definition_texts(elements_dir, path):
    """The dynamic texts of a symbol (its label, its comment...), the way
    the program copies them onto the sheet when the symbol is placed."""
    root = ET.parse(f"{elements_dir}/{path}").getroot()
    texts = []
    for text in root.iter("dynamic_text"):
        item = ET.Element("dynamic_elmt_text",
                          {k: v for k, v in text.attrib.items() if k != "z"})
        item.set("uuid", "{%s}" % uuid.uuid4())
        for child in text:
            item.append(child)
        texts.append(item)
    return texts


def definition_terminals(elements_dir, path):
    """The terminals of a symbol, in the order of its definition."""
    root = ET.parse(f"{elements_dir}/{path}").getroot()
    return [(float(t.get("x")), float(t.get("y")), t.get("orientation"))
            for t in root.iter("terminal")]


class Sheet:
    def __init__(self, elements_dir, title):
        self.elements_dir = elements_dir
        self.title = title
        self.elements = []
        self.conductors = []
        self.next_id = 0

    def place(self, path, x, y, label, comment="", orientation=0):
        """Put a symbol on the sheet; return its terminal numbers."""
        ids = []
        terminals = []
        for tx, ty, orient in definition_terminals(self.elements_dir, path):
            ids.append(self.next_id)
            terminals.append((self.next_id, tx, ty, orient))
            self.next_id += 1
        self.last_uuid = "{%s}" % uuid.uuid4()
        self.elements.append({"path": path, "x": x, "y": y,
                              "orientation": orientation, "label": label,
                              "comment": comment, "terminals": terminals,
                              "uuid": self.last_uuid, "links": []})
        return ids

    def link(self, slave_uuid, master_uuid):
        """Link a contact to its coil, as the program does both ways: the
        contact then shows the label of its coil."""
        for e in self.elements:
            if e["uuid"] == slave_uuid:
                e["links"].append(master_uuid)
            elif e["uuid"] == master_uuid:
                e["links"].append(slave_uuid)

    def wire(self, a, b):
        self.conductors.append((a, b))

    def xml(self, order):
        d = ET.Element("diagram", {
            "title": self.title, "order": str(order), "version": "0.90",
            "cols": "17", "colsize": "60", "rows": "8", "rowsize": "80",
            "height": "660", "displaycols": "true", "displayrows": "true",
            "folio": "%id", "titleblocktemplate": "default",
            "titleblocktemplateCollection": "common"})
        elements = ET.SubElement(d, "elements")
        for item in self.elements:
            path, x, y = item["path"], item["x"], item["y"]
            label, comment = item["label"], item["comment"]
            terminals = item["terminals"]
            e = ET.SubElement(elements, "element", {
                "type": "common://" + path, "x": str(x), "y": str(y),
                "orientation": str(item["orientation"]), "uuid": item["uuid"],
                "freezeLabel": "false", "z": "10"})
            ts = ET.SubElement(e, "terminals")
            for tid, tx, ty, orient in terminals:
                ET.SubElement(ts, "terminal", {
                    "id": str(tid), "x": f"{tx:g}", "y": f"{ty:g}",
                    "orientation": ORIENT[orient]})
            ET.SubElement(e, "inputs")
            if item["links"]:
                links = ET.SubElement(e, "links_uuids")
                for other in item["links"]:
                    ET.SubElement(links, "link_uuid", {"uuid": other})
            dynamic = ET.SubElement(e, "dynamic_texts")
            for text in definition_texts(self.elements_dir, path):
                dynamic.append(text)
            infos = ET.SubElement(e, "elementInformations")
            ET.SubElement(infos, "elementInformation",
                          {"name": "label", "show": "1"}).text = label
            if comment:
                ET.SubElement(infos, "elementInformation",
                              {"name": "comment", "show": "1"}).text = comment
        conductors = ET.SubElement(d, "conductors")
        for a, b in self.conductors:
            ET.SubElement(conductors, "conductor", {
                "terminal1": str(a), "terminal2": str(b), "x": "0", "y": "0",
                "num": "", "type": "multi", "condsize": "1", "numsize": "7",
                "displaytext": "1", "onetextperfolio": "0",
                "horizrotatetext": "0", "vertirotatetext": "270",
                "text_color": "#000000", "freezeLabel": "false",
                "bicolor": "false", "color2": "#000000", "dash-size": "2"})
        return d


def gv_pins(ids):
    """gv2p : terminals 5,6,4,1,3,2 -> (top L1 L2 L3, bottom L1 L2 L3)."""
    return [ids[3], ids[4], ids[0]], [ids[5], ids[2], ids[1]]


def contactor(sheet, x, y, coil_x, coil_y, label, comment):
    """The power contacts of a contactor and its coil, linked: the
    contacts show the label of the coil."""
    pins = km_pins(sheet.place(KM, x, y, label, comment))
    contacts = sheet.last_uuid
    sheet.place(COIL, coil_x, coil_y, label, comment)
    sheet.link(contacts, sheet.last_uuid)
    return pins


def km_pins(ids):
    """com_puiss4 : top L1 L2 L3, bottom L1 L2 L3."""
    return [ids[0], ids[2], ids[4]], [ids[1], ids[3], ids[5]]


def direct_start(el):
    s = Sheet(el, "Direct-on-line start")
    q_top, q_bot = gv_pins(s.place(GV, 200, 200, "-Q1", "Motor protection"))
    k_top, k_bot = contactor(s, 200, 320, 520, 320, "-KM1", "Line contactor")
    m = s.place(M3, 200, 460, "-M1", "Motor")
    m_u, m_v, m_w = m[1], m[3], m[0]
    for a, b in zip(q_bot, k_top):
        s.wire(a, b)
    for a, b in zip(k_bot, (m_u, m_v, m_w)):
        s.wire(a, b)
    return s


def star_delta_start(el):
    s = Sheet(el, "Star-delta start")
    q_top, q_bot = gv_pins(s.place(GV, 200, 160, "-Q1", "Motor protection"))
    k1_top, k1_bot = contactor(s, 200, 280, 640, 280, "-KM1", "Line contactor")
    m = s.place(M6, 200, 440, "-M1", "Motor 6 terminals")
    u2, w2, u1, w1, v1, v2 = m[0], m[1], m[2], m[3], m[4], m[5]
    k2_top, k2_bot = contactor(s, 400, 280, 720, 280, "-KM2", "Delta contactor")
    k3_top, k3_bot = contactor(s, 400, 560, 800, 280, "-KM3", "Star contactor")
    for a, b in zip(q_bot, k1_top):
        s.wire(a, b)
    for a, b in zip(k1_bot, (u1, v1, w1)):
        s.wire(a, b)
        # Delta : the supply on W2, U2, V2 through KM2, so that U1-W2,
        # V1-U2 and W1-V2 are each on one phase
    for a, b in zip(q_bot, k2_top):
        s.wire(a, b)
    for a, b in zip(k2_bot, (w2, u2, v2)):
        s.wire(a, b)
        # Star : U2, V2, W2 joined together by KM3
    for a, b in zip((u2, v2, w2), k3_top):
        s.wire(a, b)
    s.wire(k3_bot[0], k3_bot[1])
    s.wire(k3_bot[1], k3_bot[2])
    return s


def digital_io(el):
    s = Sheet(el, "Digital I/O card")
    di = s.place(DI, 100, 280, "-A1", "Digital inputs DI0-DI7, 24 V DC")
        # Each device above its channel: I0.0 at x=270, I0.1 at x=350
    pb = s.place(PB, 270, 170, "-S1", "Start")
    ind = s.place(IND, 350, 150, "-B1", "Inductive sensor, PNP")
        # Card terminals: 1 L+, 2 L-, 3..10 the channels 0..7
    s.wire(pb[0], di[0])
    s.wire(pb[1], di[2])
        # Inductive sensor: brown to L+, black (switched output) to the
        # input, blue to L-
    s.wire(ind[1], di[0])
    s.wire(ind[2], di[3])
    s.wire(ind[0], di[1])
    do = s.place(DO, 100, 420, "-A2", "Digital outputs DO0-DO7, 24 V DC")
    lamp = s.place(LAMP, 270, 560, "-H1", "Running")
    coil = s.place(COIL, 350, 560, "-K1", "Auxiliary relay")
    s.wire(do[2], lamp[0])
    s.wire(lamp[1], do[1])
    s.wire(do[3], coil[0])
    s.wire(coil[1], do[1])
    return s


def analog_io(el):
    s = Sheet(el, "Analog I/O card")
    ai = s.place(DI, 100, 300, "-A3", "Analog inputs AI0-AI7, 4-20 mA / 0-10 V")
        # Right of the channels it uses: its wires leave upwards and come
        # down to the card beside its body, never across it
    press = s.place(PRESS, 440, 210, "-B2", "Pressure transmitter 4-20 mA")
        # Between the first channel and the transmitter: its right terminal
        # goes down to the second channel, its left one to L-
    temp = s.place(TEMP, 300, 230, "-B3", "Temperature probe")
        # Transmitter: S+ (signal) to the first channel, its supply between
        # L+ and L-
    s.wire(press[0], ai[2])
    s.wire(press[1], ai[0])
    s.wire(press[2], ai[1])
        # Two-wire probe between the second channel and L-
    s.wire(temp[1], ai[3])
    s.wire(temp[0], ai[1])
    s.place(DO, 100, 460, "-A4", "Analog outputs AO0-AO7, 0-10 V")
    return s


def make_project(elements_dir, out):
    project = ET.Element("project", {"version": "0.90",
                                     "title": "FaseCAD example templates"})
    for order, make in enumerate((direct_start, star_delta_start,
                                  digital_io, analog_io), start=1):
        project.append(make(elements_dir).xml(order))
    ET.ElementTree(project).write(out, encoding="utf-8", xml_declaration=True)


NAMES = ["Direct-on-line start", "Star-delta start",
         "Digital IO card", "Analog IO card"]


def make_templates(elements_dir, project_file, out_dir):
    """Every sheet of the saved project as a .qetmak, the way
    DiagramView::createTemplateFromSelection() writes one."""
    root = ET.parse(project_file).getroot()
    for index, diagram in enumerate(root.iter("diagram")):
        macro = ET.Element("qet_macro")
        collection = ET.SubElement(macro, "collection")
        done = set()
        for element in diagram.iter("element"):
            kind = element.get("type", "")
            if not kind:
                continue
                # embed://import/10_electric/... or common://10_electric/...
            path = kind.split("://", 1)[1]
            if not path.startswith("import/"):
                path = "import/" + path
            element.set("type", "macro://" + path)
            if path in done:
                continue
            done.add(path)
            entry = ET.SubElement(collection, "element", {"path": path})
            definition = ET.parse(
                f"{elements_dir}/{path[len('import/'):]}").getroot()
            entry.append(definition)
        content = ET.SubElement(macro, "diagram_content")
        content.append(diagram)
        name = NAMES[index] if index < len(NAMES) else diagram.get("title")
        ET.indent(macro)
        ET.ElementTree(macro).write(f"{out_dir}/{name}.qetmak",
                                    encoding="utf-8", xml_declaration=False)
        print("written", name)


if __name__ == "__main__":
    if sys.argv[1] == "project":
        make_project(sys.argv[2], sys.argv[3])
    else:
        make_templates(sys.argv[2], sys.argv[3], sys.argv[4])
