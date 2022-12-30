class MidiRoutingTable {
    constructor(jsonState, stateManager) {
        this.jsonState = jsonState;
        this.state = JSON.parse(jsonState);
        this.stateManager = stateManager;
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
        headerRow.setAttribute('id', 'routing-to');

        let oldheaderRow = document.querySelector('#routing-to');
        let firstTh = document.importNode(document.querySelector('#routing-from-to'), true);
        headerRow.appendChild(firstTh);
        for (let idto in this.state.to) {
            if (this.state.to.hasOwnProperty(idto)) {
                for (let dev in this.state.attached) {
                    if (this.state.attached.hasOwnProperty(dev) && idto.includes(dev)) {
                        this.insertHeader45(headerRow, this.state.to[idto]);
                    }
                }
            }
        }
        oldheaderRow.parentElement.replaceChild(headerRow, oldheaderRow);
    }

    rebuildRoutingRows() {
        let newTbody = document.createElement('tbody');
        newTbody.setAttribute('id','routes');
        let ncols = Object.keys(this.state.to).length;
        for (let idfrom in this.state.from) {
            if (this.state.from.hasOwnProperty(idfrom)) {
                for (let dev in this.state.attached) {
                    if (this.state.attached.hasOwnProperty(dev) && idfrom.includes(dev)) {
                        let newRow = newTbody.insertRow(-1);
                        let newFrom = document.createElement('th');
                        newFrom.className = 'row-header';
                        let newText = document.createTextNode(this.state.from[idfrom]);
                        newFrom.appendChild(newText);
                        newRow.append(newFrom);
                        for (let idto in this.state.to) {
                            if (this.state.to.hasOwnProperty(idto)) {
                                let newCheckBox = document.createElement('input');
                                newCheckBox.type = 'checkbox';
                                for (let idroute in this.state.routing) {
                                    if (this.state.routing.hasOwnProperty(idroute) && idroute === this.state.from[idfrom]) {
                                        newCheckBox.checked = (this.state.routing[idroute].indexOf(this.state.to[idto]) > -1);
                                        newCheckBox.setAttribute('route-from', this.state.from[idfrom]);
                                        newCheckBox.setAttribute('route-to', this.state.to[idto]);
                                        newCheckBox.addEventListener('click', (ev) => {
                                            let args = [ev.target.getAttribute('route-from'), ev.target.getAttribute('route-to')];
                                            //console.log(ev.target.getAttribute('route-from')+ "->"+ev.target.getAttribute('route-to')+" "+ev.target.checked);
                                            if (ev.target.checked) {
                                                this.stateManager.sendCommand('con', args);
                                            }
                                            else {
                                                this.stateManager.sendCommand('dis', args);
                                            }
                                        });
                                    }
                                }
                                let newCell = newRow.insertCell(-1);
                                newCell.appendChild(newCheckBox);
                            }
                        }
                    }
                }
            }
        }
        let oldTbody = document.querySelector('#routes');
        oldTbody.parentNode.replaceChild(newTbody, oldTbody);
    }

    init() {
        this.rebuildHeaderRow();
        this.rebuildRoutingRows();
    }

    reinit(jsonState) {
        this.state = JSON.parse(jsonState);
        this.init();
    }

    postCommand(commandObject) {
        
    }
}