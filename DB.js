// Doorbell stream listener (PngMagic script)

dbIP = '192.168.0.101' // set to doorbell IP (port 80)
Url = 'http://' + dbIP + '/events'
if(!Http.Connected)
	Http.Connect( 'event', Url )  // Start the event stream

var last
mute = false

Pm.SetTimer(10*1000)
heartbeat = 0
// Handle published events
function OnCall(msg, event, data)
{
	switch(msg)
	{
		case 'HTTPDATA':
//Pm.Echo(data)
			mute = false
			heartbeat = new Date()
			if(data.length <= 2) break // keep-alive heartbeat
			lines = data.split('\n')
			for(i = 0; i < lines.length; i++)
				procLine(lines[i])
			break
		case 'HTTPCLOSE':
//			Http.Connect( 'event', Url )  // Start the event stream
			Pm.Echo('DB stream closed')
			break
	}
}

function procLine(data)
{
	data = data.replace(/\n|\r/g, "")
	if(data.length < 2) return

	if( data.indexOf( 'event' ) >= 0 )
	{
		event = data.substring( data.indexOf(':') + 2)
		return
	}
	else if( data.indexOf( 'data' ) >= 0 )
	{
		data = data.substring( data.indexOf(':') + 2)
	}
	else
	{
		return // headers
	}

	switch(event)
	{
		case 'state':
			break
		case 'print':
			Pm.Echo( 'DB Print: ' + data)
			break
		case 'alert':
			Pm.Alert( data )
			if(data.indexOf('Doorbell') == 0 )
			{
				// find a doorbell sound to play!
				Pm.PlaySound('C:\\AndroidShare\\Media\\Audio\\Notifications\\christmas-bell-large.wav')
				// Send an alert to the waterbed heater
//				Pm.WbMsg('ALERT', 'Doorbell', 1200, 2000)  // f=1200, duration=2000ms
			}
			break
		case 'request':
			dumpReq(data)
			break
		case 'hack':
			hackJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
				data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')
			Pm.Echo('DB Hack: ' + hackJson.ip + ' ' + hackJson.pass)
			break
	}
	event = ''
}

function dumpReq(data)
{
	Json = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')

	Pm.Echo('DB Req: R=' + Json.remote + ' H=' + Json.host)
	Pm.Echo('  URI=' + Json.url)
//	if(Json.header)
//	 for(i = 0; i < Json.header.length; i++)
//		Pm.Echo('  Header ' + i + ': ' +Json.header[i])
	if(Json.params)
	 for(i = 0; i < Json.params.length; i++)
		Pm.Echo('  Param ' + i + ': ' +Json.params[i])

	ip = Json.host
	if(ip.search('.com') > 0) // reduce to just IP
	{
		ip = ip.replace( /[a-z.]/g, '' )
		ip = ip.replace( /[-]/g, '.' )
		ip = ip.slice(1)
	}

	if( ValidateIPaddress(ip) && Reg.localIP != ip && ip != dbIP)
	{
		Reg.localIP = ip
		Pm.Echo('New IP: ' + Reg.localIP)
	}
}

function OnTimer()
{
	time = (new Date()).valueOf()
	if(time - heartbeat > 120*1000)
	{
		if(!Http.Connected)
		{
			if(!mute)
			{
				Pm.Echo('DB timeout')
				mute = true
			}
			Http.Connect( 'event', Url )  // Start the event stream
		}
	}
}

function ValidateIPaddress(ip) 
{  
	if (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(ip))
	{
		return (true)
	}
	return (false)
}
