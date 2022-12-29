class ConnectedDevicesTable
{
    constructor(jsonDevices, jsonPreset) {
        this.devices = JSON.parse(jsonDevices);
        this.preset = JSON.parse(jsonPreset);
        this.maxNickname = 12;
        this.userInputHandler = new UserInputHandler(this.maxNickname);
    }
    insertCell(row, text) {
        let newCell = row.insertCell(-1);
        let newText = document.createTextNode(text);
        newCell.appendChild(newText);
        return newCell;
    }
    buildTableRow(dev, presetData, dirText, new_tbody) {
        for (let dirid in presetData) {
            if (presetData.hasOwnProperty(dirid)) {
                if (dirid.includes(dev)) {
                    let new_row = new_tbody.insertRow(-1);
                    let dirLetter = dirText.slice(0,1);
                    this.insertCell(new_row, dev);
                    this.insertCell(new_row, dirid.slice(dirid.lastIndexOf(dirLetter) + 1));
                    this.insertCell(new_row, dirText);
                    let nickname = this.insertCell(new_row, presetData[dirid]);
                    nickname.contentEditable = 'true';
                    nickname.setAttribute('data-id', dirid);
                    nickname.addEventListener("beforeinput", (ev) => {
                        this.userInputHandler.handleBeforeInput(ev);
                    });
                    nickname.addEventListener("input", (ev) => {
                        this.handleInput(ev);
                    });
                    /*
                    nickname.addEventListener("keydown", (ev) => {
                        this.handleKeydown(ev);
                    });
                    nickname.addEventListener('paste', (ev) => {
                        this.handlePaste(ev);
                    })*/
                    nickname.addEventListener('click', (ev) => {
                        if (!this.inEditing(nickname)) {
                            this.startEditing(nickname);
                        }
                    });
                    this.insertCell(new_row, this.devices[dev]);
                }
            }
        }
    }

    handleKeydown(ev) {
        let deleting = (ev.code === "Backspace" || ev.code === "Delete");
        if((ev.target.firstChild.textContent.length >= this.maxNickname && !deleting) || (deleting && ev.target.firstChild.textContent.length<=0)){
            ev.preventDefault();
        }
    }

    handleInput(ev) {
        const btnRename = ev.target.querySelector('.btn-rename');
        btnRename.disabled = ev.target.firstChild.textContent.length < 1;
    }

    handlePaste(ev) {
        const data = ev.clipboardData.getData('Text');
        if (data.length > this.maxNickname) {
            console.log("data too long "+data);
            ev.preventDefault();
        }
    }

    init() {
        let new_tbody = document.createElement('tbody');
        for (let dev in this.devices) {
            if (this.devices.hasOwnProperty(dev)) {
                // dev is the USB ID. Alternately search from and
                // to lists for the same ID an populate table rows accordingly
                this.buildTableRow(dev, this.preset.from, 'FROM',new_tbody);
                this.buildTableRow(dev, this.preset.to, 'TO', new_tbody);
            }
        }
        let old_tbody = document.querySelector('#devlist');
        //console.log("BEFORE: rows="+old_tbody.rows.length);

        old_tbody.parentNode.replaceChild(new_tbody, old_tbody);
        //console.log("AFTER: rows="+new_tbody.rows.length);
        //if (new_tbody.rows.length > 0) {
        //    console.log("AFTER: cols="+new_tbody.rows[0].cells.length);
        //}
    }
    startEditing(td) {
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
        console.log(id +':'+oldnickname+'=>'+td.textContent);
    }

    inEditing(td) {
        return td.classList.contains('in-editing');
    }

    updatePreset(jsonPreset) {

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
};