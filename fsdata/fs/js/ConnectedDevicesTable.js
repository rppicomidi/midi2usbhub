class ConnectedDevicesTable
{
    constructor(jsonState, stateManager) {
        this.jsonState = jsonState;
        this.stateManager = stateManager;
        this.state = JSON.parse(jsonState);
        this.maxNickname = 12;
        this.userInputHandler = new UserInputHandler(this.maxNickname);
    }

    insertCell(row, text) {
        let newCell = row.insertCell(-1);
        let newText = document.createTextNode(text);
        newCell.appendChild(newText);
        return newCell;
    }

    addCancelPrevEditingOnClick(td) {
        td.addEventListener('click', (ev) => {
            this.cancelPrevEditing(td);
        });
    }

    insertDisplayCell(row, text) {
        let td = this.insertCell(row, text);
        this.addCancelPrevEditingOnClick(td);
    }

    buildTableRow(dev, presetData, dirText, new_tbody) {
        for (let dirid in presetData) {
            if (presetData.hasOwnProperty(dirid)) {
                if (dirid.includes(dev)) {
                    const new_row = new_tbody.insertRow(-1);
                    const dirLetter = dirText.slice(0,1);
                    this.insertDisplayCell(new_row, dev);
                    const port = dirid.slice(dirid.lastIndexOf(dirLetter) + 1);
                    this.insertDisplayCell(new_row, port);
                    this.insertDisplayCell(new_row, dirText);
                    let nickname = this.insertCell(new_row, presetData[dirid]);
                    nickname.contentEditable = 'true';
                    nickname.setAttribute('data-id', dirid);
                    nickname.addEventListener("beforeinput", (ev) => {
                        this.userInputHandler.handleBeforeInput(ev);
                    });
                    nickname.addEventListener("input", (ev) => {
                        this.handleInput(ev);
                    });
                    nickname.addEventListener('click', (ev) => {
                        if (!this.inEditing(nickname)) {
                            this.startEditing(nickname);
                        }
                    });
                    if (dev === '0000-0000') {
                        const portLetter = String.fromCharCode('A'.charCodeAt(0) + Number(port) - 1);
                        let midiDirText = ' OUT ';
                        if (dirLetter === 'F') {
                            midiDirText = ' IN ';
                        }
                        this.insertDisplayCell(new_row, 'MIDI'+midiDirText+portLetter);
                    }
                    else {
                        this.insertDisplayCell(new_row, this.state.attached[dev]);
                    }
                }
            }
        }
    }

    handleInput(ev) {
        const btnRename = ev.target.querySelector('.btn-rename');
        btnRename.disabled = (ev.target.firstChild.textContent.length < 1) ||
            ev.target.firstChild.textContent === ev.target.getAttribute('data-old-name') ||
            ev.target.firstChild.textContent.includes("?"); // The ? character is the command and argument delimeter
    }

    init() {
        let thead = document.querySelector('#connected-thead');
        let ths = thead.querySelectorAll('th');
        ths.forEach(th => {
            this.addCancelPrevEditingOnClick(th);
        })
        let new_tbody = document.createElement('tbody');
        new_tbody.setAttribute('id', 'devlist');
        for (let dev in this.state.attached) {
            if (this.state.attached.hasOwnProperty(dev)) {
                // dev is the USB ID. Alternately search from and
                // to lists for the same ID an populate table rows accordingly
                this.buildTableRow(dev, this.state.from, 'FROM',new_tbody);
                this.buildTableRow(dev, this.state.to, 'TO', new_tbody);
            }
        }
        let old_tbody = document.querySelector('#devlist');

        old_tbody.parentNode.replaceChild(new_tbody, old_tbody);
    }

    reinit(jsonState) {
        if (jsonState !== this.jsonState) {
            this.jsonState = jsonState;
            this.state = JSON.parse(jsonState);
            this.init();
        }
    }

    cancelPrevEditing(td) {
        let activeTd = this.findEditing();
        if (activeTd) {
            this.cancelEditing(activeTd);
        }
    }

    startEditing(td) {
        this.cancelPrevEditing(td);
        td.className = 'in-editing';
        td.setAttribute('data-old-name',td.textContent);
        this.createButtonToolbar(td);
    }

    cancelEditing(td) {
        td.classList.remove('in-editing');
        this.removeToolbar(td);
        td.textContent = td.getAttribute('data-old-name');
    }

    finishEditing(td) {
        td.classList.remove('in-editing');
        this.removeToolbar(td);
        const oldnickname = td.getAttribute('data-old-name');
        const id = td.getAttribute('data-id');
        this.stateManager.sendCommand('ren', [oldnickname, td.textContent]);
        td.textContent+=' renaming...';
    }

    inEditing(td) {
        return td.classList.contains('in-editing');
    }

    createButtonToolbar(td) {
        const toolbar = document.createElement('div');
        toolbar.className = 'button-toolbar';
        toolbar.setAttribute('contentEditable','false');
        toolbar.innerHTML = `
        <div class="button-wrapper">
            <button class="btn-small btn-cancel">Cancel</button>
            <button class="btn-small btn-rename">Rename</button>
        </div>
        `

        td.appendChild(toolbar);

        const btnCancel = toolbar.querySelector('.btn-cancel');
        btnCancel.addEventListener('click', (ev) => {
            ev.stopPropagation();
            this.cancelEditing(td);
        });
        const btnRename = toolbar.querySelector('.btn-rename');
        btnRename.disabled = true;
        btnRename.addEventListener('click', (ev) => {
            ev.stopPropagation();
            this.finishEditing(td);
        });
        // do not allow button elements to be selected
        toolbar.addEventListener('selectstart', (ev) => {
            ev.preventDefault();
        });
    }

    removeToolbar(td) {
        const toolbar = td.querySelector('.button-toolbar');
        toolbar.remove(toolbar);
    }

    findEditing() {
        let tbody = document.querySelector('#devlist');
        let tds = tbody.querySelectorAll('td');
        return Array.prototype.find.call(tds, td => this.inEditing(td));
    }
};