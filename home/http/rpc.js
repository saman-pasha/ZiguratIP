
var call_xhr = null;

function transaction_id()
{
    $.ajax({
	dataType: "text",
	url: "rpc.zt?do=id",
	async: false,
	error: function(xhr) {
	    $("#spn_transaction_id").html("RPC Service not respond");
	},
	success: function(data) {
	    $("#spn_transaction_id").html(data);
	}
    });
}

$(document).ready(function () {

    transaction_id();

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

	transaction_id();

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
	    url: 'rpc.zt?do=call&auto_commit=' + $("#chb_auto_commit").is(":checked") +
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

	transaction_id();
	
	$("#div_result_set").empty();
	$("#div_result_set").append("<img id=\"img_indicator\" src=\"indicator.gif\"/>");

	$("#div_result_set").load('rpc.zt?do=commit', function() {
	    $("#img_indicator").hide();
	});
    });

    $("#btn_rollback").click(function () {

	transaction_id();

	$("#div_result_set").empty();
	$("#div_result_set").append("<img id=\"img_indicator\" src=\"indicator.gif\"/>");

	$("#div_result_set").load('rpc.zt?do=rollback', function() {
	    $("#img_indicator").hide();
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
