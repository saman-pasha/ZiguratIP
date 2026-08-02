# Parsi Programming Language

Parsi is a case insensitive server side programming language which has consists of a compiler that compiles SQL-Like code to C++ code and then to machine code.
There is three way to compile Parsi code, first by passing the file path to parsi executable binary file that has placed in ZIGURATIP_HOME/bin folder.
For another way you should go to ziguratip compiler web page, may be [http://localhost:2190/compiler.zt](http://localhost:2190/compiler.zt) address.
Second way must be configured by connector.conf file in ZIGURATIP_HOME/etc.
The third way is compile method of [Connector](connector.md) library.
All objects in parsi language should have a name consists of domain address and object name like: domain::subdomain::name. domain and all sub domains are optional and only for classification. Domains works like namespaces in other languages.
Executable codes must placed in procedure and it must be called by RPC method which available in [Connector](connector.md) library or [http://localhost:2190/rpc.zt](http://localhost:2190/rpc.zt) address.
The main feature of Parsi language is to create web pages or web services for web apps or Business Process Management and Work Flow.
Go to [Page](page.md) documentation, build some pages and see the magic of parsi to writing SQL code in Web Page.
Exactly parsi is Object Oriented, supports class type, inheritance and method virtualization and overriding. But all features are related to session base of parsi.
Objects in parsi has life time which has defined in declaration time. For declaring a variable see [Declare](declare.md) documentation.
Parsi doesn't have any framework right now, but it has some syntax that helps developer to use available libararies like [Boost](https://www.boost.org/), [Qt](https://www.qt.io/developers/) and many more full featured C++ libraries.
For using external libraries you should use two INCLUDE and LINK clauses at the top of the file and use them in code.
See [C++ Helpers](cpp.md) and introduce by available C++ syntaxes and know how to use them.
