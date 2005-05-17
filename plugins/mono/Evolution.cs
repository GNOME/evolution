using System;
using System.Runtime.InteropServices;
using System.Reflection;

using Camel;

namespace Evolution {
	[StructLayout (LayoutKind.Sequential)]
	public class PopupTarget {
		public IntPtr popup;
		public IntPtr widget;
		public int type;
		public int mask;
	};

	[StructLayout (LayoutKind.Sequential)]
	public class MenuTarget {
		public IntPtr menu;
		public IntPtr widget;
		public int type;
		public int mask;
	};

	[StructLayout (LayoutKind.Sequential)]
	public class EventTarget {
		public IntPtr aevent;
		public int type;
		public int mask;
	};
};

namespace Evolution {
	public class Error {
		// can we marshal varags from c#?
		[DllImport("eutil")] static extern int e_error_run(IntPtr parent, string tag, IntPtr end);
		[DllImport("eutil")] static extern int e_error_run(IntPtr parent, string tag, string arg0, IntPtr end);
		[DllImport("eutil")] static extern int e_error_run(IntPtr parent, string tag, string arg0, string arg1, IntPtr end);
		[DllImport("eutil")] static extern int e_error_run(IntPtr parent, string tag, string arg0, string arg1, string arg2, IntPtr end);

		public static int run(IntPtr parent, string tag) {
			return e_error_run(parent, tag, (IntPtr)0);
		}
		public static int run(IntPtr parent, string tag, string arg0) {
			return e_error_run(parent, tag, arg0, (IntPtr)0);
		}
		public static int run(IntPtr parent, string tag, string arg0, string arg1) {
			return e_error_run(parent, tag, arg0, arg1, (IntPtr)0);
		}
		public static int run(IntPtr parent, string tag, string arg0, string arg1, string arg2) {
			return e_error_run(parent, tag, arg0, arg1, arg2, (IntPtr)0);
		}
	}
}

namespace Evolution.Mail {
	/* ********************************************************************** */
	[StructLayout (LayoutKind.Sequential)]
	public class PopupTargetSelect : PopupTarget {
		public IntPtr _folder;
		public string uri;
		public IntPtr _uids;

		public static PopupTargetSelect get(IntPtr o) {
			return (PopupTargetSelect)Marshal.PtrToStructure(o, typeof(PopupTargetSelect));
		}

		public Camel.Folder folder {
			get { return (Camel.Folder)Camel.Object.fromCamel(_folder); }
		}

		public string [] uids {
			get { return Camel.Util.getUIDArray(_uids); }
		}		
	}

	[StructLayout (LayoutKind.Sequential)]
	public class PopupTargetURI : Evolution.PopupTarget {
		public string uri;

		public static PopupTargetURI get(IntPtr o) {
			return (PopupTargetURI)Marshal.PtrToStructure(o, typeof(PopupTargetURI));
		}
	}

	[StructLayout (LayoutKind.Sequential)]
	public class PopupTargetPart : PopupTarget {
		public string mimeType;
		public IntPtr _part;

		public static PopupTargetPart get(IntPtr o) {
			return (PopupTargetPart)Marshal.PtrToStructure(o, typeof(PopupTargetPart));
		}

		public Camel.Object part {
			get { return (Camel.Object)Camel.Object.fromCamel(_part); }
		}
	}

	[StructLayout (LayoutKind.Sequential)]
	public struct PopupTargetFolder {
		public Evolution.PopupTarget target;
		public string uri;

		public static PopupTargetFolder get(IntPtr o) {
			return (PopupTargetFolder)Marshal.PtrToStructure(o, typeof(PopupTargetFolder));
		}
	}

	/* ********************************************************************** */
	[StructLayout (LayoutKind.Sequential)]
	public class MenuTargetSelect : MenuTarget {
		public IntPtr _folder;
		public string uri;
		public IntPtr _uids;

		public static MenuTargetSelect get(IntPtr o) {
			return (MenuTargetSelect)Marshal.PtrToStructure(o, typeof(MenuTargetSelect));
		}

		public Camel.Folder folder {
			get { return (Camel.Folder)Camel.Object.fromCamel(_folder); }
		}

		public string [] uids {
			get { return Camel.Util.getUIDArray(_uids); }
		}		
	}

	/* ********************************************************************** */
	[StructLayout (LayoutKind.Sequential)]
	public class EventTargetFolder : EventTarget {
		public string uri;

		public static EventTargetFolder get(IntPtr o) {
			return (EventTargetFolder)Marshal.PtrToStructure(o, typeof(EventTargetFolder));
		}
	}

	[StructLayout (LayoutKind.Sequential)]
	public class EventTargetMessage : EventTarget {
		public IntPtr _folder;
		public string uid;
		public IntPtr _message;

		public static EventTargetMessage get(IntPtr o) {
			return (EventTargetMessage)Marshal.PtrToStructure(o, typeof(EventTargetMessage));
		}

		public Camel.Folder folder {
			get { return (Camel.Folder)Camel.Object.fromCamel(_folder); }
		}

		public Camel.MimeMessage message {
			get { return (Camel.MimeMessage)Camel.Object.fromCamel(_message); }
		}

	}
};
