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
                    nickname.addEventListener('click', (ev) => {
                        if (!this.inEditing(nickname)) {
                            this.startEditing(nickname);
                        }
                    });
                    this.insertCell(new_row, this.state.attached[dev]);
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
        this.stateManager.sendCommand('ren', [oldnickname, td.textContent]);
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