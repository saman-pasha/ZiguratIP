
CLASS CompilerDrawer INHERITS HTMLDrawer
BEGIN
PRIVATE:
	DECLARE _code AS String;
	DECLARE _message AS String;
PUBLIC:
	CONSTRUCTOR(code AS String = '', message AS String = '')
	INITIALIZE HTMLDrawer('Zigurat Compiler', 'compiler.zt', 'post')
	BEGIN
		SET this._code = code;
		SET this._message = message;
	END

	OVERRIDE FUNCTION draw_page_header() RETURNS Void
	BEGIN
		ECHO '<div style=\"align:center; background-image: url(\'zeytun.png\'); margin: auto; width: 720px; height: 90px;\"></div>';
		ECHO '	    <div>';
		ECHO '		<p><h3>Welcome to Zigurat Compiler</h3></p>';
		ECHO '			        <ul>';
		ECHO '					<li><a href=\"zigurat.html\">Zigurat</a></li>';
		ECHO '					<li><a href=\"index.html\">Zeytun</a></li>';
		ECHO '				</ul>';
		ECHO '		<hr>';
	END
	
	OVERRIDE FUNCTION draw_content_open() RETURNS Void 
	BEGIN
		ECHO '<div style=\"color: white; background-color: #37b82d; padding: 20px;\">';
	END

	OVERRIDE FUNCTION draw_form_content() RETURNS Void 
	BEGIN
		ECHO '<span style=\"color: black;\">', this._message, '</span><br>';
		ECHO '<textarea rows=\"24\" style=\"width: 100%;\" name=\"code\">', this._code, '</textarea><br><br>';
		ECHO '<input type=\"submit\" value=\"Compile\"/>';
	END

	OVERRIDE FUNCTION draw_page_footer() RETURNS Void 
	BEGIN
		ECHO '</div>';
	END

	DESTRUCTOR()
	BEGIN

	END
END
