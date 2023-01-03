class StateManager {
    constructor(jsonState) {
        this.jsonState = jsonState;
        this.state = JSON.parse(jsonState);
        this.tbl = new ConnectedDevicesTable(jsonState, this);
        this.tblR = new MidiRoutingTable(jsonState, this);
        this.pre = new PresetManager(jsonState, this);
        this.req = undefined;
    }

    init() {
        this.tbl.init();
        this.tblR.init();
        this.pre.init();
    }

    reqMidiHubState() {
        this.req = new XMLHttpRequest();
        this.req.onload = () => { if (this.req.readyState == this.req.DONE) {
                let respCode = this.req.status;
                if (respCode == 200) {
                    try {
                        // will throw SyntaxError if not valid JSON
                        let newState =JSON.parse(this.req.responseText);
                        // re-stringify the object so we can test for equality with previous state
                        // the parson library might not stringify the same way the browser does
                        let newJsonState = JSON.stringify(newState);
                        if (newJsonState !== this.jsonState) {
                            this.state = newState;
                            this.jsonState = newJsonState;
                            this.tbl.reinit(this.jsonState);
                            this.tblR.reinit(this.jsonState);
                            this.pre.reinit(this.jsonState);
                        }
                    }
                    catch (error) {
                        console.log(error);
                    }
                }
                this.req = undefined;
                this.setup();
            }
        };
        this.req.onerror = () => {
            this.req = undefined;
            this.setup();
        };
        this.req.onabort = () => {
            this.req = undefined;
        };
        this.req.open("GET","/connected_state.json", true);
        this.req.send();
        this.timeoutId = undefined;
    }

    setup() {
        if (typeof this.timeoutId === 'number') {
            this.cancel();
        }
        this.timeoutId = setTimeout(()=>{ this.reqMidiHubState();}, 1000); // poll the LED state from the server one second from now.
    }

    cancel() {
        if (this.timeoutId != undefined) {
            clearTimeout(this.timeoutId);
        }
        if (this.req != undefined) {
            this.req.abort();
        }
    }

    sendCommand(command, args) {
        this.cancel();
        this.req = new XMLHttpRequest();
        this.req.open("POST", "/post_cmd.txt", true);
        this.req.setRequestHeader("Content-Type", "text/plain");
        this.req.onload = () => {
            let state = this.req.readyState;
            let respCode = this.req.status;
            if (state == this.req.DONE) {
                if (respCode === 201) {
                    let data = JSON.parse(this.req.responseText);
                    if (data !== null && data.hasOwnProperty('result') && data['result'] === 'OK') {
                        this.setup();
                    }
                }
                else if (respCode === 400 || respCode === 404 || respCode === 429) {
                    let newHref = '/' +respCode+'.html';
                    document.location.href= newHref;
                }
                else {
                    // command was rejected; force hub state to right itself now
                    console.log('unexpected error code'+respCode);
                    this.reqMidiHubState();
                }
            }
        };
        this.req.onerror = () => {
            // there was an error. force hub state to right itself now
            this.reqMidiHubState();
        };
        this.req.onabort = () => {
            // there was an error. force hub state to right itself now
            this.reqMidiHubState();
        };
        let cmd = command;
        for (const arg of args) {
            cmd += "?"+arg;
        }
        console.log("command="+cmd);
        this.req.send(cmd);
    }
}