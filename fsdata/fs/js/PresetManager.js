class PresetManager {
    constructor(jsonState, stateManager) {
        this.jsonState = jsonState;
        this.state = JSON.parse(jsonState);
        this.stateManager = stateManager;
        this.maxFilename = 12;
        this.userInputHandler = new UserInputHandler(this.maxFilename);
        this.saveMode='Save';
        this.loadMode='Load';
        this.renameMode='Rename';
        this.deleteMode='Delete';
        this.mode =  this.saveMode;
    }


    init() {
        this.rebuildPresetMenu();
        /*
        window.addEventListener('click', (ev) => {
            this.cancelPrevEditing();
        });*/
    }

    reinit(jsonState) {
        this.state = JSON.parse(jsonState);
        this.init();
    }

    handleModeClick() {
        this.cancelPrevEditing();
        this.createModeButtonToolbar();
    }
    /*
    save preset: show current preset if there is one and New Preset
    load preset: show all presets. Do not show New Preset
    rename preset: show all presets. Do not show New Preset
    delete preset: show all presets. Do not show New Preset
    */
    rebuildPresetMenu() {
        let old_navbar = document.querySelector('#preset-nav')
        let navbar = document.createElement('ul');
        navbar.setAttribute('id', 'preset-nav');
        navbar.classList.add('navbar');
        let firstLi = document.createElement('li');
        firstLi.setAttribute('id','preset-id');
        let firstDiv = document.createElement('div');
        firstDiv.classList.add('preset');
        firstDiv.setAttribute('id','preset-mode');
        let firstDivText = document.createTextNode('\u{25BC} ' +this.mode+ ' Presets');
        firstDiv.appendChild(firstDivText);
        firstLi.appendChild(firstDiv);
        // do not allow to be selected
        firstLi.addEventListener('selectstart', (ev) => {
            ev.preventDefault();
        });
        this.handleModeClick = this.handleModeClick.bind(this);
        firstLi.addEventListener('click', this.handleModeClick);
        navbar.appendChild(firstLi);
        this.state.allpre.forEach(preset => {
            if (preset === this.state.curpre || this.mode !== this.saveMode) {
                let newText = document.createTextNode(preset);
                let newDiv = document.createElement('div');            
                newDiv.classList.add('preset');
                if (preset === this.state.curpre) {
                    newDiv.classList.add('active');
                }
                newDiv.appendChild(newText);
                let newLi = document.createElement('li');
                newLi.appendChild(newDiv);
                navbar.appendChild(newLi);
                if (this.mode === this.renameMode) {
                    newDiv.contentEditable = 'true';
                    newDiv.addEventListener("beforeinput", (ev) => {
                        this.userInputHandler.handleBeforeInput(ev);
                    });
                    newDiv.addEventListener("input", (ev) => {
                        this.handleInput(ev);
                    });
                    newDiv.addEventListener('click', (ev)=> {
                        this.startEditing(newLi, newDiv, 'Rename', ()=> {
                            this.finishEditing(newLi, newDiv);
                            ev.stopPropagation();
                            newDiv.textContent += ' renaming...';
                        }, true);
                    });
                }
                else {
                    newDiv.addEventListener('click', (ev)=> {
                        const args = [ev.target.textContent];
                        if (this.mode === this.deleteMode) {
                            this.startEditing(newLi, newDiv, 'Delete', ()=> {
                                this.cancelEditing(newLi, newDiv);
                                ev.stopPropagation();
                                this.stateManager.sendCommand('del', args);
                                ev.target.textContent += ' deleting...';
                            }, false);
                        }
                        else if (this.mode === this.loadMode) {
                            this.cancelPrevEditing();
                            ev.target.textContent += ' loading...';
                            this.stateManager.sendCommand('lod', args);
                        }
                        else if (this.mode == this.saveMode) {
                            this.startEditing(newLi, newDiv, 'Save', ()=> {
                                this.finishEditing(newLi, newDiv);
                                ev.stopPropagation();
                                newDiv.textContent += ' saving...';
                            }, false);
                        }
                        ev.stopPropagation();
                    });
                }
            }
        });
        if (this.mode === this.saveMode) {
            let newText = document.createTextNode("New Preset");
            let newDiv = document.createElement('div');            
            newDiv.appendChild(newText);
            newDiv.classList.add('preset');
            newDiv.contentEditable = 'true';
            let newLi = document.createElement('li');
            newLi.appendChild(newDiv);
            navbar.appendChild(newLi);
            newDiv.addEventListener("beforeinput", (ev) => {
                this.userInputHandler.handleBeforeInput(ev);
            });
            newDiv.addEventListener("input", (ev) => {
                this.handleInput(ev);
            });
            newDiv.addEventListener('click', (ev) => {
                if (!this.inEditing(newLi)) {
                    this.startEditing(newLi, newDiv, 'Save', ()=> {
                        this.finishEditing(newLi, newDiv);
                        ev.stopPropagation();
                    }, true);
                    newDiv.textContent += ' saving...';
                }
            });
        }
        old_navbar.parentNode.replaceChild(navbar, old_navbar);
    }

    handleInput(ev) {
        const btnOK = ev.target.querySelector('.btn-ok');
        const prevText = ev.target.getAttribute('data-old-name');
        btnOK.disabled = (ev.target.firstChild.textContent.length < 1) ||
            prevText === ev.target.firstChild.textContent ||
            ev.target.firstChild.textContent.includes("?"); // The ? character is the command and argument delimeter
    }

    inEditing(elem) {
        return elem.classList.contains('in-editing');
    }

    cancelPrevEditing() {
        let parent = this.findEditing();
        if (parent) {
            let activeElem = parent.querySelectorAll('div');
            if (activeElem[0]) {
                console.log(activeElem[0].textContent);
                this.cancelEditing(parent, activeElem[0]);
            }
            const id = parent.getAttribute('id');
            if (id && id === 'preset-id') {
                parent.addEventListener('click', this.handleModeClick);
            }
        }
    }

    startEditing(parent, elem, okText, okFn, disableOK) {
        this.cancelPrevEditing();
        parent.classList.add('in-editing');
        elem.setAttribute('data-old-name',elem.textContent);
        this.createButtonToolbar(parent, elem, okText, okFn, disableOK);
    }

    cancelEditing(parent, elem) {
        parent.classList.remove('in-editing');
        this.removeToolbar(elem);
        if (elem.getAttribute('contentEditable'))
            elem.textContent = elem.getAttribute('data-old-name');
        else if (elem.getAttribute('id') === 'preset-id') {
            this.handleModeClick = this.handleModeClick.bind(this);
            elem.addEventListener('click', this.handleModeClick);
        }
    }

    finishEditing(parent, elem) {
        parent.classList.remove('in-editing');
        this.removeToolbar(elem);
        if (this.mode === this.saveMode) {
            this.stateManager.sendCommand('sav', [elem.textContent]);
        }
        else if (this.mode === this.renameMode) {
            const args = [elem.getAttribute('data-old-name'), elem.textContent];
            this.stateManager.sendCommand('ren', args);
        }
    }

    createButtonToolbar(parent, elem, okName, okClickFn, disableOK) {
        const toolbar = document.createElement('div');
        toolbar.className = 'button-toolbar';
        toolbar.setAttribute('contentEditable','false');
        toolbar.innerHTML = `
        <div class="button-wrapper">
            <button class="btn-small btn-cancel">Cancel</button>
            <button class="btn-small btn-ok">OK</button>
        </div>
        `

        elem.appendChild(toolbar);

        const btnCancel = toolbar.querySelector('.btn-cancel');
        btnCancel.addEventListener('click', (ev) => {
            ev.stopPropagation();
            const modesel = document.querySelector('#preset-id');
            this.cancelEditing(parent, elem);
        });
        const btnOK = toolbar.querySelector('.btn-ok');
        btnOK.textContent = okName;
        btnOK.addEventListener('click', (ev) => {
            ev.stopPropagation();
            okClickFn();
        });
        btnOK.disabled = disableOK;
        // do not allow button elements to be selected
        toolbar.addEventListener('selectstart', (ev) => {
            ev.preventDefault();
        });
    }

    createModeButtonToolbar() {
        const toolbar = document.createElement('div');
        toolbar.className = 'button-toolbar';
        toolbar.setAttribute('contentEditable','false');
        let newDiv = document.createElement('div');
        newDiv.className = 'button-wrapper';
        let saveButtonText = document.createTextNode(this.saveMode);
        let loadButtonText = document.createTextNode(this.loadMode);
        let renameButtonText = document.createTextNode(this.renameMode);
        let deleteButtonText = document.createTextNode(this.deleteMode);
        let saveButton = document.createElement('button');
        saveButton.appendChild(saveButtonText);
        saveButton.addEventListener('click', () => {
            this.mode = this.saveMode;
            this.rebuildPresetMenu();
        });
        toolbar.appendChild(saveButton);
        let loadButton = document.createElement('button');
        loadButton.appendChild(loadButtonText);
        loadButton.addEventListener('click', () => {
            this.mode = this.loadMode;
            this.rebuildPresetMenu();
        });
        toolbar.appendChild(loadButton);
        let renameButton = document.createElement('button');
        renameButton.appendChild(renameButtonText);
        renameButton.addEventListener('click', () => {
            this.mode = this.renameMode;
            this.rebuildPresetMenu();
        });
        toolbar.appendChild(renameButton);
        let deleteButton = document.createElement('button');
        deleteButton.appendChild(deleteButtonText);
        deleteButton.addEventListener('click', () => {
            this.mode = this.deleteMode;
            this.rebuildPresetMenu();
        });
        toolbar.appendChild(deleteButton);
        
        let firstDiv = document.querySelector('#preset-mode');
        firstDiv.appendChild(toolbar);
        let firstLi = document.querySelector('#preset-id');
        firstLi.removeEventListener('click', this.handleModeClick);
        firstLi.classList.add('in-editing');

        // do not allow button elements to be selected
        toolbar.addEventListener('selectstart', (ev) => {
            ev.preventDefault();
        });

    }
    removeToolbar(elem) {
        const toolbar = elem.querySelector('.button-toolbar');
        toolbar.remove(toolbar);
    }

    findEditing() {
        let lu = document.querySelector('#preset-nav');
        let lis = lu.querySelectorAll('li');
        return Array.prototype.find.call(lis, elem => this.inEditing(elem));
    }
}