class MidiRoutingTable {
    constructor(jsonDevices, jsonPreset) {
        this.devices = JSON.parse(jsonDevices);
        this.preset = JSON.parse(jsonPreset);
    }
    // need an insert column header function
    insertHeader45(row, text) {
        let newText = document.createTextNode(text);
        let newSpan = document.createElement('span');
        newSpan.appendChild(newText);
        let newDiv = document.createElement('div');
        newDiv.appendChild(newSpan);
        let newTh = document.createElement('th');
        newTh.className = 'rotate-45';
        newTh.appendChild(newDiv);
        row.appendChild(newTh);
    }

    rebuildHeaderRow() {
        let headerRow = document.createElement('tr');

        let oldheaderRow = document.querySelector('#routing-to');
        let firstTh = document.importNode(document.querySelector('#routing-from-to'), true);
        headerRow.appendChild(firstTh);
        for (let idto in this.preset.to) {
            this.insertHeader45(headerRow, this.preset.to[idto]);
        }
        oldheaderRow.parentElement.replaceChild(headerRow, oldheaderRow);
    }

    rebuildRoutingRows() {
        let newTbody = document.createElement('tbody');
        let ncols = Object.keys(this.preset.to).length;
        for (let idfrom in this.preset.from) {
            let newRow = newTbody.insertRow(-1);
            let newFrom = document.createElement('th');
            newFrom.className = 'row-header';
            let newText = document.createTextNode(this.preset.from[idfrom]);
            newFrom.appendChild(newText);
            newRow.append(newFrom);
            for (let idto in this.preset.to) {
                let newCheckBox = document.createElement('input');
                newCheckBox.type = 'checkbox';
                for (let idroute in this.preset.routing) {
                    if (idroute === this.preset.from[idfrom]) {
                        newCheckBox.checked = (this.preset.routing[idroute].indexOf(this.preset.to[idto]) > -1);
                    }
                }
                let newCell = newRow.insertCell(-1);
                newCell.appendChild(newCheckBox);
            }
        }
        let oldTbody = document.querySelector('#routes');
        oldTbody.parentNode.replaceChild(newTbody, oldTbody);
    }

    init() {
        this.rebuildHeaderRow();
        this.rebuildRoutingRows();
    }
}