( function( window )
{

	"use strict";

	if (!Object.keys) {
    Object.keys = function (obj) {
        var keys = [],
            k;
        for (k in obj) {
            if (Object.prototype.hasOwnProperty.call(obj, k)) {
                keys.push(k);
            }
        }
        return keys;
    };
}

	var err = function( jqXHR, textStatus, error )
	{
		var json = $.parseJSON( jqXHR.responseText );
		if ( typeof json !== "undefined" && typeof json.reason !== "undefined" )
			alert( json.reason );
		else
			alert( error );
	};
	
	window.Dashboard = function( tutesElem, newElem )
	{
		var dash = this;
		dash.tutesElem = tutesElem;
		dash.newElem = newElem;

		dash.init = function()
		{
					
			// Create DOM elements
			dash.tutorials = $( "<ul>" ).addClass( "tutorials ui-widget-content ui-widget" );
			dash.newTutorial = $( "<ul>" ).addClass( "tutorials ui-widget-content ui-widget" );
			$( dash.tutesElem ).append( dash.tutorials );
			$( dash.newElem ).append( dash.newTutorial );
				
			// Get tutes
			$.ajax({
				type: "GET",
				dataType: "JSON",
				url: "/tutorials/",
				success: this.draw,
				error: err 
			});
		};

		dash.draw = function( json )
		{
			new TutorialHeader( dash.tutorials );
			$( json ).each( function()
			{
				new Tutorial( dash.tutorials, this ); 
			});

			if ( Object.keys( json ).length == 0 )
			{
				$( dash.tutorials ).append( $( "<li>" ).text( "No previous tutorials submitted." ) );
			}

			new NewTutorial( dash.newTutorial );
		};

	};

	var TutorialHeader = function( ul  )
	{
		// Output header 
		var courses = $( "<li>" ).text( "Course Code(s)" );
		var date = $( "<li>" ).text( "Date" );
		var start = $( "<li>" ).text( "Start Time" );
		var finish = $( "<li>" ).text( "Finish Time" );

		$( ul ).append( $( "<li>" ).addClass( "ui-widget-header header" ).append( $( "<ul>" ).append( courses, date, start, finish ) ) );
	};

	var Tutorial = function( ul, json )
	{
		var tute = this;

		// Output tutorial
		var courses = $( "<li>" ).text( json.courses ).addClass( "courses" );
		var date = $( "<li>" ).text( json.date );
		var start = $( "<li>" ).text( json.start );
		var finish = $( "<li>" ).text( json.finish );

		$( ul ).append( $( "<li>" ).append( $( "<ul>" ).append( courses, date, start, finish ) ) );

	};

	var NewTutorial = function( ul )
	{
		var tute = this;
		tute.students = {};
		tute.studentsElem = $( "<ul>" ).addClass( "students ui-widget-content ui-widget" );
		tute.buttonsElem = $( "<ul>" ).addClass( "buttons" );

		tute.debounce = function( el, callback )
		{
			var t = null;
			$( el ).bind( 'keyup', function()
			{
				clearTimeout( t );
				var val = $( this ).val();
				t = setTimeout( function()
				{
					if ( $( el ).val() == val )
						callback();
				}, 500 );

			});
		};

		tute.load_students = function()
		{
			$.ajax({
				type: "GET",
				dataType: "JSON",
				url: "/students/",
				success: tute.add_students,
				error: err, 
				data: { courses: tute.courses.val().split(/\W/).join(";") } 
			});
		};

		tute.add_students = function( json )
		{
			$( json ).each( function ()
			{
				$.extend( tute.students, this );
			});
			tute.draw_students();
		};

		tute.draw_students_header = function()
		{
			var sn = $( "<li>" ).text( "Student Number" );
			var first = $( "<li>" ).text( "First Name" );
			var last = $( "<li>" ).text( "Last Name" );
			var college = $( "<li>" ).text( "College" );
			var attended = $( "<li>" ).text( "Attended" );

			$( tute.studentsElem ).append( $( "<li>" ).append( $( "<ul>" ).addClass( "header ui-widget-header" ).append( sn, first, last, college, attended ) ) );
		};

		tute.draw_students = function()
		{
			// Remove existing
			tute.studentsElem.empty();

			// Add header
			tute.draw_students_header();

			// Add each student
			for ( var key in tute.students )
				if ( tute.students.hasOwnProperty( key ) )
				{
					(function()	{
						var s = tute.students[key];
						s.checked = s.checked || false;
						var sn = $( "<li>" ).text( key );
						var first = $( "<li>" ).text( s.first );
						var last = $( "<li>" ).text( s.last );
						var college = $( "<li>" ).text( s.college );
						var attended = $( "<li>" );

						var cb = $( "<input>" ).attr( "type", "checkbox" ).prop( "checked", s.checked );
						$( cb ).change( function ()
						{
							s.checked = $( cb ).is( ":checked" );
						});
						attended.append( cb );

						$( tute.studentsElem ).append( $( "<li>" ).append( $( "<ul>" ).append( sn, first, last, college, attended ) ) );
					})();
				}

			if ( Object.keys( tute.students ).length == 0 )
			{
				$( tute.studentsElem ).append( $( "<li>" ).text( "No students found. Enter a course code to see students who have attended that tutorial." ) );
			}

		};

		tute.add_student = function()
		{
			$( "#add-student" ).dialog( "open" );
		};

		tute.save = function ()
		{

			var d = {};
			d.date = tute.date.val();
			d.start = tute.start.val();
			d.finish = tute.finish.val();
			d.courses = tute.courses.val().split(/\W/).join(";");

			var s = [];
			for ( var sn in tute.students )
				if ( tute.students.hasOwnProperty( sn ) && tute.students[sn].checked )
					s.push( sn + ":" + 
						"first|" + tute.students[sn].first +
						",last|" + tute.students[sn].last +
						",college|" + tute.students[sn].college );
			d.students = s.join( ";" );

			$.ajax({
				type: "POST",
				dataType: "JSON",
				url: "/save/",
				data: d,
				success: tute.done,
				error: err 
			});
		};
		
		tute.done = function ()
		{
			alert( "Saved." );	
			window.location.reload();
		};

		this.courses = $( "<input>" ).attr( { "id": "courses", "name": "courses", "type": "text", "placeholder": "ENGG1000, ENGG1300" } );
		this.date = $( "<input>" ).attr( { "id": "date", "name": "date", "type": "text" } );
		this.start = $( "<input>" ).attr( { "id": "start", "name": "start", "type": "text" } );
		this.finish = $( "<input>" ).attr( { "id": "finish", "name": "finish", "type": "text" } );
		tute.debounce( this.courses, tute.load_students );

		var submit = $( "<button>" ).text( "Submit" );
		var addStudent = $( "<button>" ).text( "Add Student" );
		addStudent.click( tute.add_student );
		submit.click( tute.save );


		$( tute.buttonsElem ).append( addStudent, submit );

		var sn = $( "<input>" ).attr( { type: "text", name: "sn", id: "sn" } ).addClass( "text ui-widget-content ui-corner-all" );
		var first = $( "<input>" ).attr( { type: "text", name: "first", id: "first" } ).addClass( "text ui-widget-content ui-corner-all" );
		var last = $( "<input>" ).attr( { type: "text", name: "last", id: "last" } ).addClass( "text ui-widget-content ui-corner-all" );
		var college = $( "<input>" ).attr( { type: "text", name: "college", id: "college" } ).addClass( "text ui-widget-content ui-corner-all" );

		$( "body" ).append( $( "<div>" ).attr( "id", "add-student" ).append( $( "<form>" ).append ( $( "<fieldset>" ).append(
			$( "<label>" ).attr( "for", "sn" ).text( "Student Number" ), sn,
			$( "<label>" ).attr( "for", "first" ).text( "First Name" ), first,
			$( "<label>" ).attr( "for", "last" ).text( "Last Name" ), last,
			$( "<label>" ).attr( "for", "college" ).text( "College" ), college
		) ) ) );
		$( "#add-student" ).dialog({
			autoOpen: false,
			height: 400,
			width: 450,
			modal: true,
			buttons: { 
				"Add Student": function()
				{
					var sn = $( "#sn" );
					var first = $( "#first" );
					var last = $( "#last" );
					var college = $( "#college" );

					// Validate
					if ( !(/[0-9]{8}/.test( sn.val() )) ) {
						alert( "Student number must contain only 8 numbers. E.G. 42123456" );
						return;
					}

					var students = {};
					students[ sn.val() ] = { "first": first.val(), "last": last.val(), "college": college.val(), "checked": true };
					$.extend( tute.students, students ); 
					tute.draw_students();
					$( this ).dialog( "close" );
					sn.val("");
					first.val("");
					last.val("");
					college.val("");
				},
				Cancel: function() { $( this ).dialog( "close" ); }
			}
		});
		
		new TutorialHeader( ul );

		$( ul ).append( $( "<li>" ).addClass( "" ).append( $( "<ul>" ).append(
			$( "<li>" ).append( this.courses ),
			$( "<li>" ).append( this.date ), 
			$( "<li>" ).append( this.start ), 
			$( "<li>" ).append( this.finish ) ) ),
			tute.studentsElem,
			tute.buttonsElem
		);
		
		var now = new Date();
		var then = new Date();
		then.setHours( then.getHours() + 1 );
		$( this.date ).datepicker( { dateFormat: "dd-mm-yy", defaultDate: now } );
		$( this.start ).timepicker( { defaultTime: time_str( now ) } );
		$( this.finish ).timepicker( { defaultTime: time_str( then ) } );

		$( this.date ).datepicker( 'setDate', now ); 
		$( this.start ).timepicker( 'setTime', time_str( now ) );
		$( this.finish ).timepicker( 'setTime', time_str( then ) );

		tute.draw_students();

	};

	var pad_str = function( i )
	{
		return ( i < 10 ) ? "0" + i : "" + i;
	};

	var time_str = function( t )
	{
		return pad_str( t.getHours() ) + ":" + pad_str( t.getMinutes() );
	};

})( window );
