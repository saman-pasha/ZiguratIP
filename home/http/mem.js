
var pagefiles = {};
var counter = 0;
var watcher = null;

$(document).ready(function () {

    $("#btn_checksum").click(function () {
	$.ajax({
	    dataType: "text",
	    url: "mem.zt?do=checksum&name=" + $("#txt_obj_name").val(),
	    success: function(data) {
		$("#txt_checksum").val(data);
	    }
	});
    });

    $("#btn_pagefiles").click(function () {

	$("#slc_hash_keys").empty();
	$.ajax({
	    dataType: "json",
	    url: "mem.zt?do=pagefiles",
	    error: function(jqXHR, textStatus, errorThrown) {
		console.write_line(textStatus);
	    },
	    success: function(data) {
		pagefiles = {};
		$.each(data, function(key, val) {
		    if (!(val['hash_key'] in pagefiles)) {
			pagefiles[val['hash_key']] = [];
			pagefiles[val['hash_key']].push(val['start']);
			$('#slc_hash_keys').append($('<option>', { 
			    text : val['hash_key']
			}));
		    } else {
			pagefiles[val['hash_key']].push(val['start']);
		    }
		});
		$("#slc_pagefiles").empty();
		$.each(pagefiles[$("#slc_hash_keys :selected").text()], function(key, val) {
		    $('#slc_pagefiles').append($('<option>', { 
			text : val
		    }));
		});
	    }
	});

    });

    $("#slc_hash_keys").change(function () {
	$("#slc_pagefiles").empty();
	$.each(pagefiles[$("#slc_hash_keys :selected").text()], function(key, val) {
	    $('#slc_pagefiles').append($('<option>', { 
		text : val
	    }));
	});
    });

    $("#btn_pointers").click(function () {
	$("#tbl_pointers").empty();
	$("#tbl_pointers").append("<img id=\"img_indicator\" src=\"indicator.gif\"/>");

	$("#tbl_pointers").load('mem.zt?do=pointers&start=' + $("#slc_pagefiles :selected").text(), function() {
	    $("#img_indicator").hide();
	});
    });

    $("#btn_attach").click(function () {
	if (watcher == null) {
	    $.ajax({
		url: "mem.zt?do=attach_watcher",
		success: function (data) {
		    if (typeof(EventSource) !== "undefined") {
			counter = counter + 1;
			$('#div_watches').append('<fieldset id="watch_' + counter.toString() + 
						 '"><legend>Watch ' + counter.toString() + '</legend></fieldset>');
			watcher = new EventSource("mem.zt?do=watch");
			$("#spn_watcher_status").text("Attached");
			watcher.onerror = function (event) {
			    watcher.close();
			    watcher = null;
			    $("#spn_watcher_status").text('SSE Watcher not respond');
			}
			watcher.onmessage = function (event) {
			    var data = JSON.parse(event.data);
			    if ($("#chb_split_watch").is(":checked")) {
				var element = $('#watch_' + counter.toString() + data.id.toString());
				if (!element.length) {
				    $('#watch_' + counter.toString()).append('<fieldset id="watch_' + counter.toString() + data.id.toString() + 
										       '"><legend>Transaction ' + data.id.toString() + '</legend></fieldset>');
				    element = $('#watch_' + counter.toString() + data.id.toString());
				}
				element.append(data.e + '<br>');
			    } else {
				$('#watch_' + counter.toString()).append('id: ' + data.id.toString() + ', event: ' + data.e + '<br>');
			    }
			}
		    } else {
			$("#spn_watcher_status").text("EventSource is not supported");
		    }
		}
	    });
	}
    });

    $("#btn_detach").click(function () {
	$.ajax({
	    url: "mem.zt?do=detach_watcher",
	    success: function (data) {
		if (watcher != null) {
		    watcher.close();
		    watcher = null;
		    $("#spn_watcher_status").text("Detached");
		}
	    }
	});
    });

});
