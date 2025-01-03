# TCL Easy Beast

### Usage

```tcl
package require EasyBeast


proc handler {req} {
	
	#set path [dict get $req path]
	#set method [dict get $req method]
	#set body [dict get $req body]
	#set headers [dict get $req headers]

	dict create \
		status 200 \
		body {hello, world!} \
		headers [dict create \
			     Content-Type text/plain]
}

::EasyBeast::serve 0.0.0.0 3000 ::handler


``` 
