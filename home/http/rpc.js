
var call_xhr = null;

// The transaction this console is working in.
//
// do=id used to report whichever transaction the worker thread happened to be
// holding, so there was nothing to remember and nothing to send back. It opens
// one now and hands back its id, and that id has to travel with every call,
// commit and rollback -- it is how the server finds the connection again, and
// how it knows the transaction is yours.
var current_tx = null;

// Opens a transaction and shows which one. Committing or rolling back ends the
// one in hand, so a fresh one is opened straight after to work in next.
function new_transaction()
{
    $.ajax({
	dataType: "text",
	url: "rpc.zt?do=id",
	async: false,
	error: function(xhr) {
	    current_tx = null;
	    $("#spn_transaction_id").html("RPC Service not respond");
	},
	success: function(data) {
	    current_tx = $.trim(data);
	    $("#spn_transaction_id").html(current_tx);
	}
    });
}

$(document).ready(function () {

    new_transaction();

    $("#btn_add").click(function () {
	$("#tbl_params").append(`
		<tr>
		<td style="width: 200px;">
		<label for="spn_param_type">Type:</label>
		<input type="hidden" value="` + $("#slc_type option:selected").val() + `" id="spn_param_type" />
		<span>` + $("#slc_type option:selected").text() + `</span>
		</td>
		<td style="width: 250px;">
		<label for="spn_param_qualifier">Qualifier:</label>
		<input type="hidden" value="` + $("#slc_qualifier option:selected").val() + `" id="spn_param_qualifier" />
		<span>` + $("#slc_qualifier option:selected").text() + `</span>
		</td>
		<td style="width: 300px;">
		<label for="txt_param_value">Value:</label>
		<input type="text" id="txt_param_value"></input>
		</td>
		<td>
		<input type="checkbox" id="chb_param_null">NULL</input>
		</td>
		<td>
		<input onClick="$(this).closest('tr').remove();" type="button" value="Remove"/>
		</td>
		</tr>`
			       );
    });

    $("#chb_ret_val").change(function () {
	if (this.checked) {
	    $("#spn_qual_ret_val").show();
	    $("#slc_type_ret_val").show();
	    $("#spn_value_ret_val").show();
	} else {
	    $("#spn_qual_ret_val").hide();
	    $("#slc_type_ret_val").hide();
	    $("#spn_value_ret_val").hide();
	}
    });

    $("#chb_ret_val").prop("checked", false);

    $("#btn_call").click(function () {

	// Calls happen *in* the transaction already open, and used to start a new
	// one here -- which was harmless when do=id only reported whatever the
	// worker thread held, and is a leak now that it opens a connection: the
	// one from page load was abandoned, still counting against the pool.
	$("#div_result_set").empty();
	$("#div_operations").hide();
	$("#div_abort").show();
	$("#div_result_set").append("<img id=\"img_indicator\" src=\"indicator.gif\"/>");
	
	var data = {};
	var param_types = [];
	var param_quals = [];
	var param_values = [];
	var param_nulls = $("input[id=chb_param_null]");

	if ($("#chb_ret_val").is(":checked") && $("#slc_type_ret_val").is(":visible")) {
	    param_types.push($("#slc_type_ret_val option:selected").val());
	    param_quals.push("R");
	    param_values.push(null);
	}

	var counter = 0;
	$("input[id=spn_param_type]").each(function() {
	    param_types.push((param_nulls[counter].checked) ? this.value + "X" : this.value);
	    counter++;
	});
	data['param_types'] = param_types;
	
	$("input[id=spn_param_qualifier]").each(function() {
	    param_quals.push(this.value)
	});
	data['param_quals'] = param_quals;
	
	$("input[id=txt_param_value]").each(function() {
	    param_values.push(this.value)
	});
	data['param_values'] = param_values;
	
	data['params_count'] = param_types.length;

	if (param_types.length == 0) {
	    data['param_types'] = ['NULL'];
	    data['param_quals'] = ['NULL'];
	    data['param_values'] = ['NULL'];
	}

	call_xhr = $.ajax({
	    method: "post",
	    dataType: "html",
	    url: 'rpc.zt?do=call&tx=' + encodeURIComponent(current_tx) +
		'&auto_commit=' + $("#chb_auto_commit").is(":checked") +
		'&iso_lvl=' + $("#slc_iso_lvl").val() + '&proc=' + $("#txt_name").val(), 
	    data: data,
	    error: function() {
		$("#div_result_set").empty();
		$("#div_result_set").append("<p>RPC Service not respond</p>");
		call_xhr = null;
	    },
	    success: function(data) {
		$("#div_result_set").html(data);
		$("#img_indicator").hide();
		$("#div_operations").show();
		$("#div_abort").hide();

		var counter = -1;
		$("input[name=byref_params]").each(function(i, eli) {
		    if (counter == -1 && $("#chb_ret_val").is(":checked") && $("#slc_type_ret_val").is(":visible")) {
			$("#spn_value_ret_val").html(eli.value);
		    } else {
			var byref_counter = 0;
			$("input[id=spn_param_qualifier]").each(function(j, elj) {
			    if ((elj.value == 'B' || elj.value == 'O') && counter == byref_counter) {
				$("input[id=txt_param_value]").eq(j).val(eli.value);
				byref_counter++;   
			    }
			});
		    }
		    counter++;
		});
	    }
	});

    });

    $("#btn_commit").click(function () {

	$("#div_result_set").empty();
	$("#div_result_set").append("<img id=\"img_indicator\" src=\"indicator.gif\"/>");

	$("#div_result_set").load('rpc.zt?do=commit&tx=' + encodeURIComponent(current_tx), function() {
	    $("#img_indicator").hide();
	    new_transaction();          // that one is finished; carry on in the next
	});
    });

    $("#btn_rollback").click(function () {

	$("#div_result_set").empty();
	$("#div_result_set").append("<img id=\"img_indicator\" src=\"indicator.gif\"/>");

	$("#div_result_set").load('rpc.zt?do=rollback&tx=' + encodeURIComponent(current_tx), function() {
	    $("#img_indicator").hide();
	    new_transaction();
	});
    });

    $("#btn_abort").click(function () {
	if (call_xhr) {
	    call_xhr.abort();
	    call_xhr = null;
	}	
	$("#img_indicator").hide();
	$("#div_operations").show();
	$("#div_abort").hide();
	$("#div_result_set").empty();
	$("#div_result_set").append("<p>RPC XHR aborted</p>");
    });

});
