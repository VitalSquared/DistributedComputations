import React, {useEffect, useState} from 'react'
import {
    Link,
    useNavigate 
} from 'react-router-dom'
import { v4 as uuidV4 } from 'uuid'
import { io } from "socket.io-client"

export default function DocList() {
    const navigate = useNavigate();
    const [socket, setSocket] = useState();
    const [docs, setDocs] = useState([]);

    useEffect(() => {
        const s = io("http://localhost:3001")
        setSocket(s)

        return () => {
            s.disconnect()
        }
    }, [])

    useEffect(() => {
        if (socket == null) return
        socket.emit('get-all-documents')
    }, [socket])

    useEffect(() => {
        if (socket == null) return

        const handler = (documents) => {
            setDocs(documents)
        }
        socket.on('receive-all-documents', handler)

        return () => {
            socket.off('receive-all-documentss', handler)
        }
    }, [socket])

    return (
        <div>
            <button
                type='button'
                onClick={() => navigate(`/documents/${uuidV4()}`)}
            >
                Create new document
            </button>

            {docs.map((document, i) => {
                    //console.log(document)
                    return (
                        <div>
                            <Link to={`/documents/${document._id}`} activeClassName="active">{document.name}</Link>
                            <br/>
                        </div>
                    )
                }
            )}
        </div>
    )
}
