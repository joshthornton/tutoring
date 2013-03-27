( function( window )
{

	"use strict";

	var T = window.Tutorial = function( el )
	{
		this.that = this; // Local reference

		// Initialize elements
		var info = $( "<ul>" );

			// Courses
		this.courses = $( "<input>" ).attr( { "id": "courses", "name": "courses", "type": "text" } );
		info.append( $( "<li>" ).append( $( "<label>" ).attr( "for", "courses" ).text( "Course Codes (space separated)" ), this.courses ) );
			
		// Date 
		this.date = $( "<input>" ).attr( { "id": "date", "name": "date", "type": "text" } );
		info.append( $( "<li>" ).append( $( "<label>" ).attr( "for", "date" ).text( "Date" ), this.date ) );
			
		// Start 
		this.start = $( "<input>" ).attr( { "id": "start", "name": "start", "type": "text" } );
		info.append( $( "<li>" ).append( $( "<label>" ).attr( "for", "start" ).text( "Start Time" ), this.start ) );
			
		// Finish 
		this.finish = $( "<input>" ).attr( { "id": "finish", "name": "finish", "type": "text" } );
		info.append( $( "<li>" ).append( $( "<label>" ).attr( "for", "finish" ).text( "Finish Time" ), this.finish ) );
			
		// Submit 
		this.submit = $( "<button>" ).text( "Submit" );
		info.append( $( "<li>" ).append( this.submit ) );
		this.submit.click( $.proxy( this.save, this ) );
			
		// Students
		var titleColumn = $( "<ul>" ).addClass( "students title" );
		this.existingStudents = $( "<ul>" ).addClass( "students" );
		this.students = $( "<ul>" ).addClass( "students" );

		// Add a single student
		this.add_student();

		// Make title column
		titleColumn.append( $( "<li>" ).append( $( "<ul>" ).append(
			$( "<li>" ).text( "Student Number" ),
			$( "<li>" ).text( "First Name" ), 
			$( "<li>" ).text( "Last Name" ), 
			$( "<li>" ).text( "Attended" )
		) ) ); 

		// Add to DOM
		el.append( info, titleColumn, this.existingStudents, this.students );

		// Add callback to courses
		this.debounce( this.courses, $.proxy( this.get_existing_students, this ) );

		// Add date pickers and time picker
		var z = this.date.Zebra_DatePicker( { "format" : "d-m-Y" } );
		$( this.start ) .timepicker( { "step" : 15, "timeFormat" : "G:i" } );
		$( this.finish ) .timepicker( { "step" : 15, "timeFormat" : "G:i" } );
	
	}

	T.prototype = {

		add_student : function()
		{
			var ul = $( "<ul>" ).addClass( "student" );
			ul.append(
				$( "<li>" ).append( $( "<input>" ).attr( { "type": "text" } ).addClass( "sn" ) ), // Student Number
				$( "<li>" ).append( $( "<input>" ).attr( { "type": "text" } ).addClass( "first" ) ), // First Name
				$( "<li>" ).append( $( "<input>" ).attr( { "type": "text" } ).addClass( "last" ) ), // Last Name
				$( "<li>" ).append( $( "<input>" ).attr( { "type": "text" } ).addClass( "college" ) ), // College
				$( "<li>" ) // Dummy
			);

			this.students.append( $( "<li>" ).append( ul ) );
		}

		,debounce : function( el, callback )
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
		}

		,get_existing_students : function()
		{
			var existing = this.existingStudents;
			$.ajax({
				data: "courses="+this.courses.val().split(/\s/).join(";"),
				dataType: "json",
				type: "GET",
				url: "/get/",
				success: function( json, textStatus, jqXHR )
				{
					$( json ).each( function()
					{
						var ul = $( "<ul>" ).addClass( "student" );		
						ul.append(
							$( "<li>" ).text( this.student ).addClass( "sn" ),
							$( "<li>" ).text( this.first ).addClass( "first" ),
							$( "<li>" ).text( this.last ).addClass( "last" ),
							$( "<li>" ).text( this.college ).addClass( "college" ),
							$( "<li>" ).append( $( "<input>" ).attr( "type", "checkbox" ).addClass( "attending" ) )
						);

						existing.append( $( "<li>" ).append( ul ) );
					});
				},
				error: function( json, textStatus, jqXHR )
				{
					if ( typeof json !== "undefined" && typeof json.reason !== "undefined" )
						window.alert( textStatus + "("+json.reason+")" );
					else
						window.alert( textStatus );
				}
			});
		}

		,save : function()
		{
			// Post data
			var post = {};

			// Info
			post.courses = clean( this.courses.val() ).split(/\s/).join(";");
			post.date = clean( this.date.val() );
			post.start = clean( this.start.val() );
			post.finish = clean( this.finish.val() );

			// Students
			var students = [];
			
			// Existing
			$( this.existingStudents ).find( ".student" ).has( ".attending:checked" ).each( function()
			{
				students.push(
					clean( $( this ).find( ".sn" ).text() ) + ":" +
					"first" + "|" + clean( $( this ).find( ".first" ).text() ) +
					"last" + "|" + clean( $( this ).find( ".last" ).text() ) +
					"college" + "|" + clean( $( this ).find( ".college" ).text() )
				);
			});
			
			// New students
			$( this.students ).find( ".student" ).each( function()
			{
				students.push(
					clean( $( this ).find( ".sn" ).val() ) + ":" +
					"first" + "|" + clean( $( this ).find( ".first" ).val() ) +
					",last" + "|" + clean( $( this ).find( ".last" ).val() ) +
					",college" + "|" + clean( $( this ).find( ".college" ).val() )
				);
			});

			post.students = students.join( ";" );

			$.ajax({
				data: post, 
				dataType: "json",
				type: "POST",
				url: "/save/",
				success: function( json, textStatus, jqXHR )
				{
					alert( "Saved." );
				},
				error: function( json, textStatus, jqXHR )
				{
					if ( typeof json !== "undefined" && typeof json.reason !== "undefined" )
						window.alert( textStatus + "("+json.reason+")" );
					else
						window.alert( textStatus );
				}
			});
			
		}

	};

	
	var clean = function( s )
	{
		return s.replace( ";\|:", "" );
	}

})( window );
