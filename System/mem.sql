
INCLUDE '"shahelper.hpp"';
INCLUDE '"utility.hpp"';

PAGE Mem
REQUIRES Connector
BEGIN
PUBLIC:
	OVERRIDE FUNCTION page_load() RETURNS Void
	BEGIN
		TRY BEGIN
			DECLARE SESSION LOCAL con AS Connector;
			DECLARE SESSION LOCAL watcher AS Connector;

			DECLARE do AS String = request.query('do');

			IF do == 'checksum' BEGIN

				CALL response.set_header('Content-Type', 'text/html');
				ECHO `Zigurat::`SHA::`checksum(`Zigurat::SHA::SHA1, `Zigurat::`Utility::`to_upper(request.query('name').`value()));

			END ELSE IF do == 'pagefiles' BEGIN
				
				CALL response.set_header('Content-Type', 'application/json');

				IF NOT con.is_open() BEGIN
					CALL con.open();
				END
				
				CALL con.function('dba_pagefiles');
				DECLARE size AS ULong = con.`client_stream().`read_std_size();
				DECLARE i AS ULong = 0ul;

				ECHO '[';
				WHILE i < size BEGIN
					ECHO '{\"hash_key\":\"', con.`client_stream().`read_std_string(), '\",\"start\":', con.`client_stream().`read_std_long(), '}';
					IF i < size - 1ul BEGIN
						ECHO ',';
					END
					SET i = i + 1ul; 
				END
				ECHO ']';

			END ELSE IF do == 'pointers' BEGIN
				
				CALL response.set_header('Content-Type', 'text/html');

				IF NOT con.is_open() BEGIN
					CALL con.open();
				END
				
				CALL con.function('dba_pointers');
				CALL con.`client_stream().`write_std_long(request.query('start').to_long().`value());

				DECLARE a_type AS UByte;
				DECLARE a_time AS Timestamp;
				DECLARE a_state AS UByte;
				DECLARE a_lock AS UByte;
				
				WHILE con.`client_stream().`read_std_bool() BEGIN
					SET a_type = con.`client_stream().`read_std_ubyte();
					IF a_type == UByte(0) BEGIN
						ECHO '<tr class=\"free-row\">';
					END ELSE IF a_type == 1 BEGIN
						ECHO '<tr class=\"data-row\">';
					END ELSE BEGIN
						ECHO '<tr class=\"hash-row\">';
					END
					ECHO '<td>', con.`client_stream().`read_std_long(), '</td>';
					ECHO '<td>', con.`client_stream().`read_std_long(), '</td>';
					ECHO '<td>', con.`client_stream().`read_std_long(), '</td>';
					IF a_type == UByte(1) BEGIN
						SET a_state = con.`client_stream().`read_std_ubyte();
						IF a_state == UByte(0) BEGIN
							ECHO '<td></td>';
						END ELSE IF a_state == 4 BEGIN
							ECHO '<td>INSERTED</td>';
						END ELSE IF a_state == 8 BEGIN
							ECHO '<td>UPDATED</td>';
						END ELSE BEGIN
							ECHO '<td>DELETED</td>';
						END

						SET a_lock = con.`client_stream().`read_std_ubyte();
						IF a_lock == UByte(0) BEGIN
							ECHO '<td></td>';
						END ELSE IF a_lock == 1 BEGIN
							ECHO '<td class=\"shared-lock\">SHARED</td>';
						END ELSE BEGIN
							ECHO '<td class=\"exclusive-lock\">EXCLUSIVE</td>';
						END

						SET a_state = con.`client_stream().`read_std_ubyte();
						IF a_state == UByte(0) BEGIN
							ECHO '<td></td>';
						END ELSE IF a_state == 4 BEGIN
							ECHO '<td>INSERTED</td>';
						END ELSE IF a_state == 8 BEGIN
							ECHO '<td>UPDATED</td>';
						END ELSE BEGIN
							ECHO '<td>DELETED</td>';
						END

						SET a_lock = con.`client_stream().`read_std_ubyte();
						IF a_lock == UByte(0) BEGIN
							ECHO '<td></td>';
						END ELSE IF a_lock == 1 BEGIN
							ECHO '<td class=\"shared-lock\">SHARED</td>';
						END ELSE BEGIN
							ECHO '<td class=\"exclusive-lock\">EXCLUSIVE</td>';
						END

						SET a_time = con.`client_stream().`read_std_time();
						IF a_time == 0l BEGIN
							ECHO '<td></td>';
						END ELSE BEGIN
							ECHO '<td>', a_time.`to_std_string(), '</td>';
						END

						ECHO '<td>', con.`client_stream().`read_std_size(), '</td>';
						ECHO '<td>', con.`client_stream().`read_std_long(), '</td>';
						ECHO '<td>', con.`client_stream().`read_std_long(), '</td>';

						SET a_time = con.`client_stream().`read_std_time();
						IF a_time == 0l BEGIN
							ECHO '<td></td>';
						END ELSE BEGIN
							ECHO '<td>', a_time.to_string(), '</td>';
						END

						SET a_time = con.`client_stream().`read_std_time();
						IF a_time == 0l BEGIN
							ECHO '<td></td>';
						END ELSE BEGIN
							ECHO '<td>', a_time.to_string(), '</td>';
						END
					END ELSE BEGIN
						ECHO '<td colspan=\"10\" style=\"text-align:center\">N/A</td>';
					END
					ECHO '</tr>';
				END

			END ELSE IF do == 'attach_watcher' BEGIN

				IF NOT watcher.is_open() BEGIN
					CALL watcher.open();
				END
				
				CALL watcher.function('dba_attach_watcher');

			END ELSE IF do == 'detach_watcher' BEGIN

				IF NOT con.is_open() BEGIN
					CALL con.open();
				END
				
				CALL con.function('dba_detach_watcher');
				CALL watcher.close();

			END ELSE IF do == 'watch' BEGIN

				CALL response.set_header('Content-Type', 'text/event-stream');
				CALL response.set_header('Cache-Control', 'no-cache');
				CALL response.flush();

				DECLARE transaction_id AS ULong;
				WHILE TRUE BEGIN
					SET transaction_id = watcher.`client_stream().`read_std_size();
					IF transaction_id > 0ul BEGIN
						ECHO 'data:{\"id\":', transaction_id, ',\"e\":\"', watcher.`client_stream().`read_std_string(), '\"}\n\n';
						CALL response.flush();
					END ELSE BEGIN
						BREAK;
					END
				END

			END
		END CATCH ex AS Exception BEGIN
			ECHO '{\"Exception\": \"', ex.code(), ' ', ex.message(), '\"}';
		END
	END

	DESTRUCTOR ()
	BEGIN

	END
END
