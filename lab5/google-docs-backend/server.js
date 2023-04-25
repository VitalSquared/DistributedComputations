const mongoose = require("mongoose")
const Document = require("./Document")

mongoose.connect("mongodb://127.0.0.1:27017/google-docs")

const io = require('socket.io')(3001, {
    cors: {
        origin: 'http://localhost:3000',
        methods: ['GET', 'POST']
    }
})

const defaultValue = ""
const defaultName = "New document"

io.on("connection", socket => {
    socket.on("get-document", async documentId => {
        const document = await findOrCreateDocument(documentId)
        socket.join(documentId)
        socket.emit("load-document", document)
    
        socket.on("send-changes", delta => {
            socket.broadcast.to(documentId).emit("receive-changes", delta)
        })

        socket.on("send-title", title => {
            socket.broadcast.to(documentId).emit("receive-title", title)
        })
    
        socket.on("save-document", async content => {
            await Document.findByIdAndUpdate(documentId, { name: content.name, data: content.data })
        })
    })

    socket.on("get-all-documents", async () => {
        const allFilter = {};
        const documents = await Document.find(allFilter)
        socket.emit("receive-all-documents", documents)
    })
})

async function findOrCreateDocument(id) {
    if (id == null) return
    
    const document = await Document.findById(id)
    if (document) {
        return document
    }

    return await Document.create({ _id: id,  name: defaultName, data: defaultValue})
}
