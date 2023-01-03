class PresetManager {
    constructor(jsonState, stateManager) {
        this.jsonState = jsonState;
        this.state = JSON.parse(jsonState);
        this.stateManager = stateManager;
        this.maxFilename = 12;
        this.userInputHandler = new UserInputHandler(this.maxFilename);
    }


    init() {
        this.rebuildPresetMenu();
    }

    reinit(jsonState) {
        this.state = JSON.parse(jsonState);
        this.init();
    }

    rebuildPresetMenu() {
        let old_navbar = document.querySelector('#preset-nav')
        let navbar = document.createElement('ul');
        navbar.setAttribute('id', 'preset-nav');
        navbar.classList.add('navbar');
        let firstLi = document.importNode(document.querySelector('#preset-id'), true);
        navbar.appendChild(firstLi);
        this.state.allpre.forEach(preset => {
            console.log("preset="+preset);
            let newText = document.createTextNode(preset);
            let newDiv = document.createElement('div');            
            newDiv.classList.add('preset');
            if (preset === this.state.curpre) {
                newDiv.classList.add('active');
            }
            newDiv.appendChild(newText);
            newDiv.addEventListener('click', (ev)=> {
                if (ev.target.getAttribute('contentEditable')) {
                    console.log('TODO: rename');
                }
                else {
                    let args = [ev.target.textContent];
                    this.stateManager.sendCommand('lod', args);
                }
            });
            let newLi = document.createElement('li');
            newLi.appendChild(newDiv);
            navbar.appendChild(newLi);
        });
        let newText = document.createTextNode("New Preset");
        let newDiv = document.createElement('div');            
        newDiv.appendChild(newText);
        newDiv.classList.add('preset');
        newDiv.contentEditable = 'true';
        newDiv.addEventListener("beforeinput", (ev) => {
            this.userInputHandler.handleBeforeInput(ev);
        });
        newDiv.addEventListener("input", (ev) => {
            this.handleInput(ev);
        });
        newDiv.addEventListener('click', (ev) => {
            if (!this.inEditing(newDiv)) {
                this.startEditing(newDiv, 'Save', ()=> {
                    console.log("Save button pressed", newDiv.textContent);
                    this.stateManager.sendCommand('sav', [newDiv.textContent]);
                });
            }
        });
        let newLi = document.createElement('li');
        newLi.appendChild(newDiv);
        navbar.appendChild(newLi);
        old_navbar.parentNode.replaceChild(navbar, old_navbar);
    }

    handleInput(ev) {
        const btnOK = ev.target.querySelector('.btn-ok');
        btnOK.disabled = (ev.target.firstChild.textContent.length < 1) ||
            ev.target.firstChild.textContent.includes("?"); // The ? character is the command and argument delimeter
    }

    inEditing(elem) {
        return elem.classList.contains('in-editing');
    }

    cancelPrevEditing(elem) {
        let activeElem = this.findEditing();
        if (activeElem) {
            this.cancelEditing(activeElem);
        }
    }

    startEditing(elem, okText, okFn) {
        this.cancelPrevEditing();
        elem.classList.add('in-editing');
        elem.setAttribute('data-old-name',elem.textContent);
        this.createButtonToolbar(elem, okText, okFn);
    }

    cancelEditing(elem) {
        elem.classList.remove('in-editing');
        this.removeToolbar(elem);
        elem.textContent = elem.getAttribute('data-old-name');
    }

    finishEditing(elem) {
        elem.classList.remove('in-editing');
        this.removeToolbar(elem);
    }

    createButtonToolbar(elem, name, okClickFn) {
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
            this.cancelEditing(elem);
        });
        const btnOK = toolbar.querySelector('.btn-ok');
        btnOK.textContent = name;
        btnOK.addEventListener('click', (ev) => {
            ev.stopPropagation();
            this.finishEditing(elem);
            okClickFn();
        });
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