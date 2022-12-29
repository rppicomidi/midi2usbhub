class UserInputHandler {
    constructor(maxchars) {
        this.maxchars = maxchars;
    }
    stripHtmlFromText(text) {
        let doc = new DOMParser().parseFromString(text, 'text/html');
        return doc.body.textContent || "";
    }
    stripLinebreaksFromText(text) {
        return text.replace(/[\r\n]+/gm, "");
    }
    handleBeforeInput(ev) {
        let itype = ev.inputType;
        if (!itype.includes("history") && !itype.includes("delete")) {
            if (itype.includes("format")) {
                // don't allow formatting the input
                ev.preventDefault();
            }
            else if (itype.includes("insert")) {
                let data = ev.data;
                let datalen = 0;
                let requireFixup = false;
                if (data === undefined || data === null) {
                    if (ev.dataTransfer) {
                        data = ev.dataTransfer.getData('text/plain');
                        // don't allow paste of anything but plain text
                        if (data && ev.dataTransfer.types.length > 1 && ev.dataTransfer.types[0] !== 'text/plain') {
                            requireFixup = true;
                        }
                    }
                    else {
                        console.log('no ev.dataTransfer');
                        data = null;
                    }
                }
                if (data) {
                    // make sure that if the input is accepted, the data will fit
                    datalen = data.length;
                    let currentLen = ev.target.firstChild.textContent.length;
                    let sel = document.getSelection();
                    let selLeft = 0;
                    let selRight = 0;
                    let newTextContent = "";
                    if (sel.type === 'Range') {
                        if (sel.focusOffset > sel.anchorOffset) {
                            // initialize selLeft & selRight
                            selLeft = sel.anchorOffset;
                            selRight = sel.focusOffset;
                        }
                        else {
                            selLeft = sel.focusOffset;
                            selRight = sel.anchorOffset;
                        }
                        // insert will overwrite selected text
                        currentLen -= (selRight - selLeft);
                    }
                    else if (sel.type == 'Caret') {
                        selLeft = sel.anchorOffset;
                        selRight = selLeft;
                    }
                    else {
                        // probably should not be sel.type === 'none', but if it is, add text to the end
                        selLeft = currentLen;
                        selRight = selLeft;
                        console.log(sel.type);
                    }
                    if ((datalen + currentLen) > this.maxchars) {
                        // new text input will make text too long; cancel the insert
                        ev.preventDefault();
                        console.log(currentLen + " " +datalen);
                    }
                    else {
                        newTextContent = ev.target.firstChild.textContent.slice(0, selLeft) + data + ev.target.firstChild.textContent.slice(selRight);
                        let cleanText = this.stripHtmlFromText(newTextContent);
                        cleanText = this.stripLinebreaksFromText(cleanText);
                        if (cleanText !== newTextContent) {
                            newTextContent = cleanText;
                            requireFixup = true;
                            console.log('characters cleaned from input');
                        }
                    }
                    if (requireFixup) {
                        // either someone tried to paste HTML or RTF text, or someone typed in HTML manually
                        ev.target.firstChild.textContent = newTextContent;
                        ev.preventDefault();
                    }
                }
                else {
                    // probably the 'enter' key or trying to paste an image or something
                    ev.preventDefault();
                }
            }
            else {
                // not sure what this is
                console.log('unexpected inputType='+itype);
                ev.preventDefault();
            }
        }
    }
    // else is history or delete. just do the default action
}