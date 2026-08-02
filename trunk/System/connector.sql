
INCLUDE '"connector.h"';
LINK '-lConnector';

CLASS Connector BASE `Zigurat::`Connector
REQUIRES IsolationLevel, ResultType
BEGIN
PUBLIC:
	CONSTRUCTOR() 
	INITIALIZE `Zigurat::`Connector()
	BEGIN

	END

	CONSTRUCTOR(path AS String) 
	INITIALIZE `Zigurat::`Connector(path.`value())
	BEGIN

	END

	CONSTRUCTOR(host AS String, port AS Int, timeout AS Int = 60) 
	INITIALIZE `Zigurat::`Connector(host.`value(), port.`value(), timeout.`value())
	BEGIN

	END

	FUNCTION open() RETURNS Void
	BEGIN
		CALL this.`open();
	END

	FUNCTION is_open() RETURNS Bool
	BEGIN
		RETURN this.`is_open();
	END

	FUNCTION close() RETURNS Void
	BEGIN
		CALL this.`close();
	END

	FUNCTION result() RETURNS ResultType
	BEGIN
		RETURN CAST<ResultType>(CAST<`uint8_t>(this.`result()));
	END

	FUNCTION function(func AS String) RETURNS Void
	BEGIN
		CALL this.`function(func.`value());
	END

	FUNCTION echo(text AS Text) RETURNS Void
	BEGIN
		CALL this.`echo(text.`value());
	END

	FUNCTION compile(code AS Text) RETURNS Void
	BEGIN
		CALL this.`compile(code.`value());
	END

	FUNCTION auto_commit(auto_commit AS Bool) RETURNS Void
	BEGIN
		CALL this.`auto_commit(auto_commit.`value());
	END
	
	FUNCTION isolate(isolation_level AS IsolationLevel) RETURNS Void
	BEGIN
		CALL this.`isolate(CAST<`Zigurat::`IsolationLevel>(CAST<`uint8_t>(isolation_level)));
	END
	
	FUNCTION call(procedure AS String) RETURNS Void
	BEGIN
		CALL this.`call(procedure.`value());
	END

	FUNCTION commit() RETURNS Void
	BEGIN
		CALL this.`commit();
	END

	FUNCTION rollback() RETURNS Void
	BEGIN
		CALL this.`rollback();
	END

	FUNCTION columns() RETURNS Vector<String>
	BEGIN
		RETURN this.`columns();
	END

	DESTRUCTOR()
	BEGIN

	END
END
