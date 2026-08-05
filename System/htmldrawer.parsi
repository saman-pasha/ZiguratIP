
CLASS HTMLDrawer
BEGIN
PROTECTED:
	DECLARE _title AS String;
	DECLARE _action AS String;
	DECLARE _method AS String;
	DECLARE _is_form AS Bool;
PUBLIC:
	CONSTRUCTOR(title AS String)
	BEGIN
		SET this._title = title;
		SET this._is_form = FALSE;
	END

	CONSTRUCTOR(title AS String, action AS String, method AS String)
	BEGIN
		SET this._title = title;
		SET this._action = action;
		SET this._method = method;
		SET this._is_form = TRUE;
	END
	
	VIRTUAL FUNCTION draw_html_open() RETURNS Void 
	BEGIN
		ECHO '<html>';
	END

	VIRTUAL FUNCTION draw_head_open() RETURNS Void 
	BEGIN
		ECHO '<head>';
	END

	VIRTUAL FUNCTION draw_title() RETURNS Void 
	BEGIN
		ECHO '<title>', this._title, '</title>';
	END

	VIRTUAL FUNCTION draw_head_content() RETURNS Void 
	BEGIN
	END

	VIRTUAL FUNCTION draw_head_close() RETURNS Void 
	BEGIN
		ECHO '</head>';
	END
	
	VIRTUAL FUNCTION draw_body_open() RETURNS Void 
	BEGIN
		ECHO '<body>';
	END

	VIRTUAL FUNCTION draw_page_header() RETURNS Void 
	BEGIN
		ECHO '<div></div>';
	END

	VIRTUAL FUNCTION draw_content_open() RETURNS Void 
	BEGIN
		ECHO '<div>';
	END

	VIRTUAL FUNCTION draw_content_close() RETURNS Void 
	BEGIN
		ECHO '</div>';
	END
	
	VIRTUAL FUNCTION draw_form_open() RETURNS Void 
	BEGIN
		ECHO '<form action=\"', this._action, '\" method=\"', this._method, '\">';
	END

	VIRTUAL FUNCTION draw_form_content() RETURNS Void 
	BEGIN
	END

	VIRTUAL FUNCTION draw_form_close() RETURNS Void 
	BEGIN
		ECHO '</form>';
	END
	
	VIRTUAL FUNCTION draw_page_footer() RETURNS Void 
	BEGIN
		ECHO '<div></div>';
	END

	VIRTUAL FUNCTION draw_body_close() RETURNS Void 
	BEGIN
		ECHO '</body>';
	END

	VIRTUAL FUNCTION draw_html_close() RETURNS Void 
	BEGIN
		ECHO '</html>';
	END

	VIRTUAL FUNCTION draw() RETURNS Void 
	BEGIN
		CALL this.draw_html_open();
		CALL this.draw_head_open();
		CALL this.draw_title();
		CALL this.draw_head_content();
		CALL this.draw_head_close();
		CALL this.draw_body_open();
		CALL this.draw_page_header();
		CALL this.draw_content_open();
		IF this._is_form BEGIN
			CALL this.draw_form_open();
			CALL this.draw_form_content();
			CALL this.draw_form_close();
		END
		CALL this.draw_content_close();
		CALL this.draw_page_footer();
		CALL this.draw_body_close();
		CALL this.draw_html_close();
	END

	DESTRUCTOR()
	BEGIN

	END
END
