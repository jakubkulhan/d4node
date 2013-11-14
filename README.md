# d4node

Simple UDP messaging program with (sort of) auto-discovery.

## Help

	Usage: d4node [ -h ] <id> <cfg>
	
	    -h     show this help
	    <id>   choose this node ID (0-255)
	    <cfg>  path to config file
	
	
	CONFIGURATION
	
	    Every line is one statement. There are 2 kinds of statements.
	
	        node <id> <port> [ <ip> ]
	
	            <id>    ID of node in the network
	            <port>  port the node binds to
	            <ip>    IP the node bind to, IP is optional meaning the node is bound to localhost
	
	        link <id-client> <id-server>
	
	            <id-client>  ID of the client node
	            <id-server>  ID of the server node
	
	        trace_timeout <millis>
	
	            <millis>  number of milliseconds after which network is retraced
	
	        resend_timeout <millis>
	
	            <millis>  number of milliseconds after which message is tried to be sent again
	
	        acknowledge_timeout <millis>
	
	            <millis>  number of milliseconds after which acknowledge is considered to be delivered

## License

The MIT license.

	Copyright (c) 2013 Jakub Kulhan <jakub.kulhan@gmail.com>
	
	Permission is hereby granted, free of charge, to any person
	obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without
	restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following
	conditions:
	
	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.
