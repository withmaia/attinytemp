fs = require 'fs'
polar = require 'polar'

# Wrapping html with header & footer
templated = (html) ->
    header_html = fs.readFileSync 'html/header.html'
    footer_html = fs.readFileSync 'html/footer.html'
    header_html + html + footer_html

slowly = (fn) -> setTimeout fn, Math.random()*1000+100

app = polar port: 10080

# Connection and registration status
connection_status = 'unconfigured'
registration_status = 'unregistered'

app.get '/connection.json', (req, res) ->
    slowly -> res.json {status: connection_status}

app.get '/registration.json', (req, res) ->
    slowly ->
        if registration_status == 'registered'
            res.json status: registration_status, email: 'test@gmail.com'
        else
            res.json status: registration_status

app.get '/scan.json', (req, res) ->
    slowly -> res.json [{ssid: '<SSID Hidden>'}]

app.post '/connect.json', (req, res) ->
    {ssid, pass} = req.body
    connection_status = 'connecting'
    updateConnection = ->
        connection_status = 'connected'
    setTimeout updateConnection, 3500
    res.end 'ok'

app.post '/register.json', (req, res) ->
    {username, password} = req.body
    registration_status = 'registering'
    updateRegistration = ->
        registration_status = 'registered'
    setTimeout updateRegistration, 3500
    res.end 'ok'

app.get '/', (req, res) ->
    res.setHeader 'Content-Type', 'text/html'
    res.end templated fs.readFileSync 'html/index.html'

app.get '/register', (req, res) ->
    res.setHeader 'Content-Type', 'text/html'
    res.end templated fs.readFileSync 'html/register.html'

app.start()
