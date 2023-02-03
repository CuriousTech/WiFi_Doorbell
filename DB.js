// Doorbell stream listener (PngMagic script)

dbIP = '192.168.31.178' // set to doorbell IP (port 80)
Url = 'ws://' + dbIP + '/ws'
if(!Http.Connected)
	Http.Connect( 'event', Url )  // Start the event stream

password  = 'password'
var last =""

Pm.SetTimer(10*1000)
heartbeat = 0
// Handle published events
function OnCall(msg, event, data)
{
	switch(msg)
	{
		case 'HTTPCONNECTED':
			Pm.Echo('DB connected')
			break;
		case 'HTTPDATA':
			mute = false
			heartbeat = new Date()
			if(data.length <= 2) break // keep-alive heartbeat
//	Pm.Echo('DB  ' + data)
			lines = data.split('\n')
			for(ln = 0; ln < lines.length; ln++)
				procLine(lines[ln])
			break
		case 'HTTPSTATUS':
	Pm.Echo('DB status ' + data)
			break
		case 'HTTPCLOSE':
			Pm.Echo('DB disconnected ' + data)
			if(heartbeat)
				Pm.Echo('DB stream closed ' + data)
			heartbeat = 0
			break
		case 'LIGHT':
			SetVar('light', 50)
			SetVar('lighttime', 13)
			break
		default:
			Pm.Echo('DB def ' + msg + ' ' + event + ' ' + data)
			break
	}
}

function SetVar(v, val)
{
	Http.Send( '{key:' + password + ',' + v + ':' + val + '}'  )
}

function procLine(data)
{
	if(data.length < 2) return
	data = data.replace(/\n|\r/g, "")
	Json = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
				data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')

	switch(Json.cmd)
	{
		case 'state':
			Reg.dbWeather = Json.weather
			Reg.dbAlert = Json.alert
/*			if(Json.bellCount)
			{
				Pm.Echo('DB bellCnt: ' + Json.bellCount)
				for(i = 0; i < Json.bellCount; i++ )
				{
					Pm.Echo('bell ' + new Date(Json.db[i]*1000) )
				}
			}
*/			break
		case 'print':
			date = new Date()
			Pm.Echo( 'DB Print: ' + date +  '  ' + Json.text)
			break
		case 'alert':
			date = new Date()
			Pm.Echo( 'DB Alert: ' + date +  '  ' + Json.text)
			if(data != last)
				Pm.Alert( Json.text )
			last = data
//Pm.Echo('Alert = ' + data.length)
			if(data.indexOf('Doorbell') == 0 )
			{
				// find a doorbell sound to play!
				Pm.PlaySound('C:\\AndroidShare\\Media\\Audio\\Notifications\\christmas-bell-large.wav')
				// Send an alert to the waterbed heater
//				Pm.WbMsg('ALERT', 1200, 2000)  // f=1200, duration=2000ms
//				Pm.PushBullet('PUSH', 'Doorbell', Json.text)
			}
			break
		case 'request':
		case 'remote':
//Pm.Echo(data)
			dumpReq(data)
			break
		case 'hack':
			Pm.Echo('DB Hack: ' + Json.ip + ' ' + Json.pass)
			break
	}
	event = ''
}

function dumpReq(data)
{
	Json = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')

	Pm.Echo('DB Req: IP=' + Json.remote + ' H=' + Json.host)
	Pm.Echo('  URI=' + Json.url)
	if( Json.url != '/')
		Pm.Log( 'DB.log', 'IP=' + Json.remote + ' H=' + Json.host + ' URI=' + Json.url )

//	if(Json.header)
//	 for(i = 0; i < Json.header.length; i++)
//		Pm.Echo('  Header ' + i + ': ' +Json.header[i])
	if(Json.params)
	 for(i = 0; i < Json.params.length; i++)
		Pm.Echo('  Param ' + i + ': ' +Json.params[i])

	ip = Json.host
	if(typeof(ip) != 'string')
		return

	if(ip.search('.com') > 0) // reduce to just IP
	{
		ip = ip.replace( /[a-z.]/g, '' )
		ip = ip.replace( /[-]/g, '.' )
		ip = ip.slice(1)
	}

	if(ip.search(':') > 0) // remove port
		ip = ip.split(':')[0]

	n0 = ip.split('.')[0]

	if( ValidateIPaddress(ip) && Reg.localIP != ip && ip != dbIP && n0 == 74)
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
			Http.Connect( 'event', Url )  // Start the event stream
	}
}

function ValidateIPaddress(ip) 
{  
	if (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(ip))
	{
		return true
	}
	return false
}
