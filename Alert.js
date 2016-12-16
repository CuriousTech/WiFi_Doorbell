// PngMagic alert window (called from DB,js)
Pm.Window( 'Alert')

OnCall('*** ...This is a sample alert...\\u000ALine 2')
timeout = 12
Pm.SetTimer( 1000 * 60)

function OnCall(data)
{
	Pm.Window( 'Alert')
	Gdi.Width = 400
	lines = data.split('\\u000A')

	for(i=0; i < lines.length-1; i++)  // remove double text
	{
		for(j=i+1; j < lines.length; j++)
		{
			if( lines[i].length && lines[i] == lines[j])
				lines[j] = ''
		}
	}
	while(lines[0] == '') lines.shift() // remove leading blank lines
	while(lines[lines.length-1] == '') lines.pop() // remove trailing blank lines

	Gdi.Height = 28 + (lines.length * 13)
	Gdi.Clear(Gdi.Argb(140, 0,0,0) )

	Gdi.Brush( Gdi.Argb( 200, 0, 0, 0) )
	Gdi.FillRectangle(0, 0, Gdi.Width, 18)		// drag bar
	Gdi.Pen( Gdi.Argb(200, 0, 0, 255) )
	Gdi.Rectangle(0, 0, Gdi.Width-1, Gdi.Height-1) // border

	Gdi.Brush( Gdi.Argb( 255, 255, 0, 0) )
	Gdi.Font( 'Arial' , 12, 'Bold')
	Gdi.Text('X',Gdi.Width-13, 2)			// close X thing

	Gdi.Brush( Gdi.Argb( 255, 0, 0, 0) )
	y = 20
	for(i=0; i < lines.length; i++)
	{
		Gdi.Text( lines[i], 6, y )
		y += 13
	}

	Gdi.Brush( Gdi.Argb( 255, 255, 255, 255) )
	y = 19
	for(i=0; i < lines.length; i++)
	{
		Gdi.Text( lines[i], 5, y )
		y += 13
	}

	timeout = 12
}

function OnTimer()
{
	if(timeout == 0)
		return
	if(--timeout == 0)
		Pm.Halt()
}
