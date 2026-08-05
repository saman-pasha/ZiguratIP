
PAGE Catalog::Pages::objects
REQUIRES Catalog::Tables::Objects
BEGIN
	OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void
	BEGIN
		ECHO '
			<table style="width: 100%">
				<thead>
					<tr>
						<th>ID</th>
		     	     			<th>TYPE</th>
		     	     			<th>DOMAIN</th>
		     	     			<th>NAME</th>
		     	     			<th>CODE</th>
		     			</tr>
				</thead>
				<tbody>';	
		SELECT '<tr>' AS 'rt, tr',
		       '<td>', obj_id, '</td>',
		       '<td>', obj_type, '</td>',
		       '<td>', obj_domain, '</td>',
		       '<td>', obj_name, '</td>',
		       '<td>', obj_code, '</td>',
		       '</tr>'
		FROM Catalog::Tables::Objects;		
		ECHO '</tbody>';	
		ECHO '</table>';	
	END

	DESTRUCTOR()
	BEGIN

	END
END

TYPE objs AS Catalog::Pages::objects;
