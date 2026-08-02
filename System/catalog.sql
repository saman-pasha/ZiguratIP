
TABLE Catalog::Tables::Objects
BEGIN
	COLUMN obj_id AS Long PRIMARY KEY;
	COLUMN obj_type AS Char INDEX KEY NOT NULL;
	COLUMN obj_domain AS String NOT NULL;
	COLUMN obj_name AS String NOT NULL;
	COLUMN obj_code AS Text NULL DEFAULT 'NOT DEFINED';
	UNIQUE KEY (obj_domain, obj_name);
END

SEQUENCE Catalog::Sequences::Objects_obj_id
BEGIN
	FROM 1; 
	TO Long::MAX;
	STEP 1;
END

PROCEDURE Catalog::Procedures::create_object(obj_type AS Char, 
	  				     obj_domain AS String, 
	  			             obj_name AS String, 
					     obj_code AS Text)
RETURNS Long
REQUIRES Catalog::Tables::Objects,
	 Catalog::Sequences::Objects_obj_id
BEGIN
	DECLARE obj_id AS Long = Catalog::Sequences::Objects_obj_id::NEXT();
	INSERT INTO Catalog::Tables::Objects
	VALUES (@obj_id, @obj_type, @obj_domain, @obj_name, @obj_code);
	RETURN obj_id;
END

PROCEDURE Catalog::Procedures::read_objects() 
RETURNS Void
REQUIRES Catalog::Tables::Objects
BEGIN
	SELECT obj_id,
	       obj_type,
	       obj_domain,
	       obj_name,
	       obj_code
	FROM Catalog::Tables::Objects;
END

PROCEDURE Catalog::Procedures::read_object_by_id(obj_id AS Long)
RETURNS Void
REQUIRES Catalog::Tables::Objects
BEGIN
	SELECT obj_id,
	       obj_type,
	       obj_domain,
	       obj_name,
	       obj_code
	FROM Catalog::Tables::Objects
	WHERE obj_id == @obj_id;
END

PROCEDURE Catalog::Procedures::read_objects_by_type(obj_type AS Char) 
RETURNS Void
REQUIRES Catalog::Tables::Objects
BEGIN
	SELECT obj_id,
	       obj_type,
	       obj_domain,
	       obj_name,
	       obj_code
	FROM Catalog::Tables::Objects
	WHERE obj_type == @obj_type;
END

PROCEDURE Catalog::Procedures::read_objects_by_domain(obj_domain AS String)
RETURNS Void
REQUIRES Catalog::Tables::Objects
BEGIN
	SELECT obj_id,
	       obj_type,
	       obj_domain,
	       obj_name,
	       obj_code
	FROM Catalog::Tables::Objects
	WHERE obj_domain == @obj_domain;
END

PROCEDURE Catalog::Procedures::read_object_by_domain_name(obj_domain AS String, obj_name AS String)
RETURNS Void
REQUIRES Catalog::Tables::Objects
BEGIN
	SELECT obj_id,
	       obj_type,
	       obj_domain,
	       obj_name,
	       obj_code
	FROM Catalog::Tables::Objects
	WHERE obj_domain == @obj_domain AND obj_name == @obj_name;
END

PROCEDURE Catalog::Procedures::update_object(obj_id AS Long, obj_code AS Text) 
RETURNS Void
REQUIRES Catalog::Tables::Objects
BEGIN
	UPDATE Catalog::Tables::Objects
	SET obj_code = @obj_code
	WHERE obj_id == @obj_id;
END

PROCEDURE Catalog::Procedures::delete_object(obj_id AS Long)
RETURNS Void
REQUIRES Catalog::Tables::Objects
BEGIN
	DELETE FROM Catalog::Tables::Objects
	WHERE obj_id == @obj_id;
END
