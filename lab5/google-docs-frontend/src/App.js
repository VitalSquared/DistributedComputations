import TextEditor from './TextEditor'
import DocList from './DocList'
import {
    BrowserRouter as Router,
    Routes,
    Route
} from 'react-router-dom'

function App() {
    return (
        <Router>
            <Routes>
                <Route path="/" exact element={<DocList/>} />
                <Route path="documents/:id" element={<TextEditor/>} />
            </Routes>
        </Router>
    )
}

export default App;
