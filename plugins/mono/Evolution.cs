using System;
using System.Runtime.InteropServices;
using System.Reflection;

using Camel;

[StructLayout (LayoutKind.Sequential)]
struct EMPopupTargetSelect {
	int type;
	int mask;
	IntPtr parent;
	IntPtr folder;
	string folderURI;
	IntPtr uids;
};

[StructLayout (LayoutKind.Sequential)]
struct EMPopupTargetFolder {
	int type;
	int mask;
	IntPtr parent;
	string folderURI;
};


[StructLayout (LayoutKind.Sequential)]
struct aCamelObject {
IntPtr klass;
uint magic;
IntPtr hooks;
uint bitfield1;
// ref_count:24
uint ref_count { // highly non-portable
	get { return (bitfield1 & 0xffffff) >> 0; }
	set { bitfield1 = (bitfield1 & 0xff000000) | ((value << 0) & 0xffffff); }
}
// flags:8
uint flags { // highly non-portable
	get { return (bitfield1 & 0xff000000) >> 24; }
	set { bitfield1 = (bitfield1 & 0xffffff) | ((value << 24) & 0xff000000); }
}
IntPtr next;
IntPtr prev;
}

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
