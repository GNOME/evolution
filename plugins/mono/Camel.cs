
using System;
using System.Collections;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace Camel {
	[StructLayout (LayoutKind.Sequential)]
	public struct CamelException {
		public int id;
		public string desc;
	}

	public class Arg {
		public enum Tag : uint {
			END = 0,
			IGNORE = 1,
			FIRST = 1024,

			TYPE = 0xf0000000, /* type field for tags */
			TAG = 0x0fffffff, /* tag field for args */

			OBJ = 0x00000000, /* object */
			INT = 0x10000000, /* int */
			DBL = 0x20000000, /* double */
			STR = 0x30000000, /* c string */
			PTR = 0x40000000, /* ptr */
			BOO = 0x50000000 /* bool */
		}
	}

	public class Exception : System.ApplicationException {
		public enum Type {
			NONE = 0,
			SYSTEM = 1
		}

		public Type id;
		public string desc;

		public Exception(CamelException ex) {
			id = (Type)ex.id;
			desc = ex.desc;
		}

		public Exception(Type _id, string _desc) {
			id = _id;
			desc = _desc;
		}
	}

	public class Util {
		[DllImport("camel-1.2")] static extern int camel_init(string certdir, bool nss);

		public static void Init(string certdir, bool nss) {
			if (camel_init(certdir, nss) != 0)
				throw new Exception(Exception.Type.SYSTEM, "Init failure");
		}

		public static string [] getUIDArray(IntPtr o) {
			GPtrArray pa = (GPtrArray)Marshal.PtrToStructure(o, typeof(GPtrArray));
			string [] uids = new string[pa.len];

			for (int i=0;i<pa.len;i++) {
				IntPtr x = Marshal.ReadIntPtr(pa.pdata, i * Marshal.SizeOf(typeof(IntPtr)));
				uids[i] = Marshal.PtrToStringAuto(x);
			}

			return uids;
		}
/*
		public static IntPtr setUIDs(string [] uids) {
			
		}
*/
		public struct UIDArray {
			public string [] uids;
			public int len;

			public UIDArray(string [] _uids) {
				uids = _uids;
				len = _uids.Length;
			}
			
			public UIDArray(IntPtr raw) {
				uids = new string[0];
				len = 0;
				Marshal.PtrToStructure(raw, this);
			}
		}
	}

	public class Object {
		// should be library scope
		public IntPtr cobject;
		private int finaliseID = -1;

		protected EventHandlerList events = new EventHandlerList();

		// reffing & wrapping stuff.
		struct CamelObject {
			public IntPtr klass;
		}

		struct CamelObjectClass {
			public IntPtr parent;
			int magic;
			IntPtr next;
			IntPtr prev;
			public string name;
		};

		private static Hashtable types = new Hashtable();
		private static Hashtable objects = new Hashtable();

		[DllImport("camel-1.2")] static extern void camel_object_ref(IntPtr raw);
		[DllImport("camel-1.2")] static extern void camel_object_unref(IntPtr raw);

		public Object(IntPtr raw) {
			// ok this is a hack around c# crap to do with unargumented constructors.
			// we can bypass to a null raw so we can properly instantiate new types
			if (raw != (IntPtr)0) {
				cobject = raw;
				toCamel(this);
			}
		}

		public Object() {
			// this is invalid?
		}

		~Object() {
			System.Console.WriteLine("object disposed " + cobject + " type " + this);

			// well we can never get a finalised event anyway ...
			if (finalise_id != -1)
				camel_object_remove_event(cobject, finalise_id);
			if (meta_changed_id != -1)
				camel_object_remove_event(cobject, meta_changed_id);

			objects.Remove(cobject);
			camel_object_remove_event(cobject, finaliseID);
			finaliseID = -1;
			camel_object_unref(cobject);
			cobject = (IntPtr)0;

			// FIXME: remove any event hooks too
		}

		static Object() {
			types.Add("CamelObject", typeof(Camel.Object));
			types.Add("CamelSession", typeof(Camel.Session));
			types.Add("CamelFolder", typeof(Camel.Folder));
			types.Add("CamelDataWrapper", typeof(Camel.DataWrapper));
			types.Add("CamelMedium", typeof(Camel.Medium));
			types.Add("CamelMimeMessage", typeof(Camel.MimeMessage));
			types.Add("CamelMimePart", typeof(Camel.MimePart));
			// camelmultipart?
			types.Add("CamelStore", typeof(Camel.Store));
			types.Add("CamelTransport", typeof(Camel.Transport));
			types.Add("CamelAddress", typeof(Camel.Address));
			types.Add("CamelInternetAddress", typeof(Camel.InternetAddress));
			types.Add("CamelStream", typeof(Camel.Stream));
			types.Add("CamelStreamMem", typeof(Camel.StreamMem));
			types.Add("CamelStreamFs", typeof(Camel.StreamFS));
		}

		public static void objectFinalised(IntPtr o, IntPtr info, IntPtr data) {
			System.Console.WriteLine("object finalised " + o);
			objects.Remove(o);
		}

		public static Object fromCamel(IntPtr raw) {
			CamelObject o;
			CamelObjectClass klass;
			WeakReference weak = (WeakReference)objects[raw];

			System.Console.WriteLine("object from camel " + raw);

			if (weak != null)
				return (Object)weak.Target;

			o = (CamelObject)Marshal.PtrToStructure(raw, typeof(CamelObject));
			if ((object)o == null)
				return null;

			klass = (CamelObjectClass)Marshal.PtrToStructure(o.klass, typeof(CamelObjectClass));
			while ((object)klass != null) {
				Console.WriteLine(" checking is " + klass.name);
				if (types.ContainsKey(klass.name)) {
					Console.WriteLine("  yep!");
					camel_object_ref(raw);
					return (Camel.Object)Activator.CreateInstance((Type)types[klass.name], new object [] { raw });
				}

				klass = (CamelObjectClass)Marshal.PtrToStructure(klass.parent, typeof(CamelObjectClass));
			}

			Console.WriteLine("  unknown type?");
			camel_object_unref(raw);
			return null;
		}

		/* this just registers an object created on the cil side */
		public static void toCamel(Object res) {
			System.Console.WriteLine("object to camel " + res.cobject);

			objects.Add(res.cobject, new WeakReference(res));
			res.finaliseID = camel_object_hook_event(res.cobject, "finalize", (CamelEventFunc)objectFinalised, (IntPtr)0);
		}

		// Camel event Wrapper and helpers
		public delegate void CamelEventFunc(IntPtr o, IntPtr info, IntPtr data);

		[DllImport("camel-1.2")] public static extern int camel_object_hook_event(IntPtr raw, string name, CamelEventFunc func, IntPtr data);
		[DllImport("camel-1.2")] public static extern void camel_object_remove_event(IntPtr raw, int id);

		protected void addEvent(String name, ref int hookid, CamelEventFunc hook, Delegate value) {
			if (hookid == -1)
				hookid = camel_object_hook_event(cobject, name, hook, (IntPtr)0);
			events.AddHandler(name, value);
		}

		protected void removeEvent(String name, ref int hookid, Delegate value) {
			events.RemoveHandler(name, value);
			if (events[name] == null) {
				camel_object_remove_event(cobject, hookid);
				hookid = -1;
			}
		}	

		// object events
		public delegate void FinaliseEvent(Camel.Object o);
		public delegate void MetaChangedEvent(Camel.Object o, String name);

		// how to remove these, at dispose time?
		private int finalise_id = -1;
		private int meta_changed_id = -1;

		private static void finaliseHook(IntPtr co, IntPtr info, IntPtr data) {
			Object o = fromCamel(co);
			FinaliseEvent f;

			if (o != null
			    && (f = (FinaliseEvent)o.events["finalize"]) != null)
				f(o);
		}

		private static void metaChangedHook(IntPtr co, IntPtr info, IntPtr data) {
			Object o = fromCamel(co);
			MetaChangedEvent f;

			if (o != null
			    && (f = (MetaChangedEvent)o.events["finalize"]) != null)
				f(o, Marshal.PtrToStringAnsi(info));
		}

		public event FinaliseEvent Finalise {
			add { addEvent("finalize", ref finalise_id, (CamelEventFunc)finaliseHook, value); }
			remove { removeEvent("finalize", ref finalise_id, value); }
		}

		public event MetaChangedEvent MetaChanged {
			add { addEvent("meta_changed", ref meta_changed_id, (CamelEventFunc)metaChangedHook, value); }
			remove { removeEvent("meta_changed", ref meta_changed_id, value); }
		}

		[DllImport("camel-1.2")] static extern IntPtr camel_object_get_ptr(IntPtr raw, ref CamelException ex, int tag);
		[DllImport("camel-1.2")] static extern void camel_object_free(IntPtr raw, int tag, IntPtr val);
		[DllImport("camel-1.2")] static extern int camel_object_get_int(IntPtr raw, ref CamelException ex, int tag);

		// maybe we want an indexer class to get properties?
		// e.g. name = folder.properties[Folder.Tag.NAME]
		public String getString(int type) {
			String s;
			IntPtr o;
			CamelException ex = new CamelException();

			o = camel_object_get_ptr(cobject, ref ex, type);
			if (ex.id != 0)
				throw new Camel.Exception(ex);

			s = Marshal.PtrToStringAuto(o);
			camel_object_free(cobject, type, o);

			return s;
		}

		public Camel.Object getObject(int type) {
			IntPtr o;
			Camel.Object co;
			CamelException ex = new CamelException();

			o = camel_object_get_ptr(cobject, ref ex, type);
			if (ex.id != 0)
				throw new Camel.Exception(ex);

			co = fromCamel(o);
			camel_object_free(cobject, type, o);

			return co;
		}

		public int getInt(int type) {
			int r;
			CamelException ex = new CamelException();

			r = camel_object_get_int(cobject, ref ex, type);
			if (ex.id != 0)
				throw new Camel.Exception(ex);

			return r;
		}

		// meta-data
		[DllImport("camel-1.2")] static extern String camel_object_meta_get(IntPtr raw, string name);
		[DllImport("camel-1.2")] static extern bool camel_object_meta_set(IntPtr raw, string name, string value);

		public String metaGet(String name) {
			return camel_object_meta_get(cobject, name);
		}

		public bool metaSet(String name, String value) {
			return camel_object_meta_set(cobject, name, value);
		}
	}

	public class Provider {
		public enum Type {
			STORE = 0,
			TRANSPORT = 1
		}
	}

	public class Session : Object {
		public Session(IntPtr raw) : base(raw) { }

		[DllImport("camel-provider-1.2")] static extern IntPtr camel_session_get_service(IntPtr o, string uri, int type, ref CamelException ex);
		[DllImport("camel-provider-1.2")] static extern IntPtr camel_session_get_service_connected(IntPtr o, string uri, int type, ref CamelException ex);

		public Service getService(string uri, Provider.Type type) {
			IntPtr s;
			CamelException ex = new CamelException();

			s = camel_session_get_service(cobject, uri, (int)type, ref ex);
			if (ex.id != 0)
				throw new Camel.Exception(ex);

			return (Service)fromCamel(s);
		}
	}

	public class Service : Object {
		public Service(IntPtr raw) : base(raw) { }
		// wrap service shit
	}

	public class Store : Service {
		public Store(IntPtr raw) : base(raw) { }

		[DllImport("camel-provider-1.2")]
                static extern IntPtr camel_store_get_folder(IntPtr o, string name, int flags, ref CamelException ex);

		Folder getFolder(string name, int flags) {
			IntPtr s;
			CamelException ex = new CamelException();

			s = camel_store_get_folder(cobject, name, flags, ref ex);
			if (ex.id != 0)
				throw new Camel.Exception(ex);

			return (Folder)fromCamel(s);
		}

		void createFolder(string name) {
		}
	}

	public class Transport : Service {
		public Transport(IntPtr raw) : base(raw) { }

		// send to (message, from, reciepients);
	}

	public class Folder : Camel.Object {
		public Folder(IntPtr raw) : base(raw) { }

		~Folder() {
			if (changed_id != -1)
				camel_object_remove_event(cobject, changed_id);
		}

		public enum Tag {
			NAME = 0x1400 + Arg.Tag.STR,
			FULL_NAME = 0x1401 + Arg.Tag.STR,
			STORE = 0x1402 + Arg.Tag.OBJ,
			PERMANENTFLAGS = 0x1403 + Arg.Tag.INT,
			TOTAL = 0x1404 + Arg.Tag.INT,
			UNREAD = 0x1405 + Arg.Tag.INT,
			DELETED = 0x1406 + Arg.Tag.INT,
			JUNKED = 0x1407 + Arg.Tag.INT,
			VISIBLE = 0x1408 + Arg.Tag.INT,
			UID_ARRAY = 0x1409 + Arg.Tag.PTR,
			INFO_ARRAY = 0x140a + Arg.Tag.PTR, // GPtrArray
			PROPERTIES = 0x140b + Arg.Tag.PTR, // GSList of properties
		}

		[DllImport("camel-provider-1.2")] static extern IntPtr camel_folder_get_message(IntPtr o, string uid, ref CamelException ex);
		[DllImport("camel-provider-1.2")] static extern IntPtr camel_folder_get_uids(IntPtr o);
		[DllImport("camel-provider-1.2")] static extern void camel_folder_free_uids(IntPtr o, IntPtr uids);
		[DllImport("camel-provider-1.2")] static extern IntPtr camel_folder_search_by_expression(IntPtr o, string expr, ref CamelException ex);
		[DllImport("camel-provider-1.2")] static extern IntPtr camel_folder_search_by_uids(IntPtr o, string expr, ref Util.UIDArray uids, ref CamelException ex);
		[DllImport("camel-provider-1.2")] static extern void camel_folder_search_free(IntPtr o, IntPtr uids);

		[DllImport("camel-provider-1.2")] static extern IntPtr camel_folder_get_message_info(IntPtr raw, String uid);

		public MimeMessage getMessage(string uid) {
			CamelException ex = new CamelException();
			IntPtr o = camel_folder_get_message(cobject, uid, ref ex);

			if (ex.id != 0)
				throw new Camel.Exception(ex);

			return (MimeMessage)fromCamel(o);
		}

		public MessageInfo getMessageInfo(string uid) {
			IntPtr o = camel_folder_get_message_info(cobject, uid);

			if (o == (IntPtr)0)
				return null;
			else
				return new MessageInfo(o);
		}

		public string [] getUIDs() {
			IntPtr o = camel_folder_get_uids(cobject);
			Util.UIDArray uids = new Util.UIDArray(o);

			camel_folder_free_uids(cobject, o);

			return uids.uids;
		}

		public string [] search(string expr) {
			CamelException ex = new CamelException();
			IntPtr o = camel_folder_search_by_expression(cobject, expr, ref ex);
			Util.UIDArray uids;

			if (ex.id != 0)
				throw new Camel.Exception(ex);

			uids = new Util.UIDArray(o);
			camel_folder_search_free(cobject, o);

			return uids.uids;
		}

		public string [] searchUIDs(string expr, string [] sub) {
			CamelException ex = new CamelException();
			Util.UIDArray uids = new Util.UIDArray(sub);
			IntPtr o = camel_folder_search_by_uids(cobject, expr, ref uids, ref ex);

			if (ex.id != 0)
				throw new Camel.Exception(ex);

			uids = new Util.UIDArray(o);
			camel_folder_search_free(cobject, o);

			return uids.uids;
		}

		public String name {
			get { return getString((int)Folder.Tag.NAME); }
		}

		public String fullName {
			get { return getString((int)Folder.Tag.FULL_NAME); }
		}

		public Camel.Store store {
			get { return (Camel.Store)getObject((int)Folder.Tag.STORE); }
		}

		// Folder events
		public delegate void ChangedEvent(Camel.Folder f);

		private int changed_id = -1;

		private static void changedHook(IntPtr co, IntPtr info, IntPtr data) {
			Camel.Folder o = (Camel.Folder)fromCamel(co);
			ChangedEvent f;
			
			Console.WriteLine("changed hook called for: " + o.cobject);

			if (o != null
			    && (f = (ChangedEvent)o.events["folder_changed"]) != null)
				f(o);
		}

		public event ChangedEvent Changed {
			add { addEvent("folder_changed", ref changed_id, (CamelEventFunc)changedHook, value); }
			remove { removeEvent("folder_changed", ref changed_id, value); }
		}
	}

	public class DataWrapper : Camel.Object {
		public DataWrapper(IntPtr raw) : base(raw) { }

		[DllImport("camel-1.2")] static extern int camel_data_wrapper_write_to_stream(IntPtr o, IntPtr s);
		[DllImport("camel-1.2")] static extern int camel_data_wrapper_decode_to_stream(IntPtr o, IntPtr s);
		[DllImport("camel-1.2")] static extern int camel_data_wrapper_construct_from_stream(IntPtr o, IntPtr s);

		public void writeToStream(Camel.Stream stream) {
			int res;

			res = camel_data_wrapper_write_to_stream(cobject, stream.cobject);
			if (res == -1)
				throw new Exception(Exception.Type.SYSTEM, "IO Error");
		}

		public void decodeToStream(Camel.Stream stream) {
			int res;

			res = camel_data_wrapper_decode_to_stream(cobject, stream.cobject);
			if (res == -1)
				throw new Exception(Exception.Type.SYSTEM, "IO Error");
		}

		public void constructFromStream(Camel.Stream stream) {
			int res;

			res = camel_data_wrapper_construct_from_stream(cobject, stream.cobject);
			if (res == -1)
				throw new Exception(Exception.Type.SYSTEM, "IO Error");
		}
	}

	public class Medium : Camel.DataWrapper {
		public Medium(IntPtr raw) : base(raw) { }

		[DllImport("camel-1.2")] static extern IntPtr camel_medium_get_content_object(IntPtr o);
		[DllImport("camel-1.2")] static extern void camel_medium_set_content_object(IntPtr o, IntPtr s);

		public DataWrapper content {
			get {
				IntPtr o = camel_medium_get_content_object(cobject);

				if (o != (IntPtr)0)
					return (DataWrapper)Object.fromCamel(o);
				else
					return null;
			}
			set {
				camel_medium_set_content_object(cobject, value.cobject);
			}
		}
	}

	public class MimePart : Camel.Medium {
		[DllImport("camel-1.2")] static extern IntPtr camel_mime_part_new();
		[DllImport("camel-1.2")] static extern IntPtr camel_mime_part_get_description(IntPtr o);
		[DllImport("camel-1.2")] static extern void camel_mime_part_set_description(IntPtr o, string s);
		[DllImport("camel-1.2")] static extern IntPtr camel_mime_part_get_disposition(IntPtr o);
		[DllImport("camel-1.2")] static extern void camel_mime_part_set_disposition(IntPtr o, string s);
		[DllImport("camel-1.2")] static extern IntPtr camel_mime_part_get_filename(IntPtr o);
		[DllImport("camel-1.2")] static extern void camel_mime_part_set_filename(IntPtr o, string s);

		public MimePart(IntPtr raw) : base(raw) { }

		public string description {
			get { return Marshal.PtrToStringAuto(camel_mime_part_get_description(cobject)); }
			set { camel_mime_part_set_description(cobject, value); }
		}

		public string disposition {
			get { return Marshal.PtrToStringAuto(camel_mime_part_get_disposition(cobject)); }
			set { camel_mime_part_set_disposition(cobject, value); }
		}

		public string filename {
			get { return Marshal.PtrToStringAuto(camel_mime_part_get_filename(cobject)); }
			set { camel_mime_part_set_filename(cobject, value); }
		}

		// FIXME: finish
	}

	public class MimeMessage : Camel.MimePart {
		[DllImport("camel-1.2")] static extern IntPtr camel_mime_message_new();
		[DllImport("camel-1.2")] static extern IntPtr camel_mime_message_get_subject(IntPtr o);
		[DllImport("camel-1.2")] static extern void camel_mime_message_set_subject(IntPtr o, string s);
		[DllImport("camel-1.2")] static extern IntPtr camel_mime_message_get_from(IntPtr o);
		[DllImport("camel-1.2")] static extern void camel_mime_message_set_from(IntPtr o, IntPtr s);
		[DllImport("camel-1.2")] static extern IntPtr camel_mime_message_get_recipients(IntPtr o, string type);
		[DllImport("camel-1.2")] static extern void camel_mime_message_set_recipients(IntPtr o, string type, IntPtr s);

		public MimeMessage(IntPtr raw) : base(raw) { }

		/* We need to use factories to create new objects otherwise the parent will instantiate an instance
		   of itself instead during the constructor setup */
		public MimeMessage() : base((IntPtr)0) {
			cobject = camel_mime_message_new();
			toCamel(this);
		}

		public string subject {
			get { return Marshal.PtrToStringAuto(camel_mime_message_get_subject(cobject)); }
			set { camel_mime_message_set_subject(cobject, value); }
		}

		public InternetAddress from {
			get { return new InternetAddress(camel_mime_message_get_from(cobject)); }
			set { camel_mime_message_set_from(cobject, value.cobject); }
		}
		
		public InternetAddress to {
			get { return new InternetAddress(camel_mime_message_get_recipients(cobject, "to")); }
			set { camel_mime_message_set_recipients(cobject, "to", value.cobject); }
		}

		public InternetAddress cc {
			get { return new InternetAddress(camel_mime_message_get_recipients(cobject, "cc")); }
			set { camel_mime_message_set_recipients(cobject, "cc", value.cobject); }
		}

		public InternetAddress bcc {
			get { return new InternetAddress(camel_mime_message_get_recipients(cobject, "bcc")); }
			set { camel_mime_message_set_recipients(cobject, "bcc", value.cobject); }
		}

		public InternetAddress resentTO {
			get { return new InternetAddress(camel_mime_message_get_recipients(cobject, "resent-to")); }
			set { camel_mime_message_set_recipients(cobject, "resent-to", value.cobject); }
		}

		public InternetAddress resentCC {
			get { return new InternetAddress(camel_mime_message_get_recipients(cobject, "resent-cc")); }
			set { camel_mime_message_set_recipients(cobject, "resent-cc", value.cobject); }
		}

		public InternetAddress resentBCC {
			get { return new InternetAddress(camel_mime_message_get_recipients(cobject, "resent-bcc")); }
			set { camel_mime_message_set_recipients(cobject, "resent-bcc", value.cobject); }
		}
	}

	// subclass real streams?  or real stream interfaces?
	public class Stream : Camel.Object {
		public Stream(IntPtr raw) : base(raw) { }

		[DllImport("camel-1.2")] static extern int camel_stream_write(IntPtr o, byte [] data, int len);
		[DllImport("camel-1.2")] static extern int camel_stream_read(IntPtr o, byte [] data, int len);
		[DllImport("camel-1.2")] static extern int camel_stream_eos(IntPtr o);
		[DllImport("camel-1.2")] static extern int camel_stream_close(IntPtr o);
		[DllImport("camel-1.2")] static extern int camel_stream_flush(IntPtr o);
		[DllImport("camel-1.2")] static extern int camel_stream_reset(IntPtr o);

		public int write(byte [] data, int len) {
			int ret;

			ret = camel_stream_write(cobject, data, len);
			if (ret == -1)
				throw new Exception(Exception.Type.SYSTEM, "IO write Error");

			return ret;
		}

		public int write(string value) {
			int ret;
			byte [] data;
			System.Text.UTF8Encoding enc = new System.Text.UTF8Encoding();

			data = enc.GetBytes(value);
			ret = camel_stream_write(cobject, data, data.Length);
			if (ret == -1)
				throw new Exception(Exception.Type.SYSTEM, "IO write Error");

			return ret;
		}


		public int read(byte [] data, int len) {
			int ret;

			ret = camel_stream_read(cobject, data, len);
			if (ret == -1)
				throw new Exception(Exception.Type.SYSTEM, "IO read Error");

			return ret;
		}

		public void close() {
			if (camel_stream_close(cobject) == -1)
				throw new Exception(Exception.Type.SYSTEM, "IO close Error");
		}

		public void reset() {
			if (camel_stream_reset(cobject) == -1)
				throw new Exception(Exception.Type.SYSTEM, "IO reset Error");
		}

		public void flush() {
			if (camel_stream_flush(cobject) == -1)
				throw new Exception(Exception.Type.SYSTEM, "IO close Error");
		}

		public bool eos() {
			return (camel_stream_eos(cobject) != 0);
		}
	}

	public class SeekableStream : Camel.Stream {
		public SeekableStream(IntPtr raw) : base(raw) { }
	}

	public class StreamFS : Camel.SeekableStream {
		public enum Flags {
			O_RDONLY = 00,
			O_WRONLY = 01,
			O_RDWR   = 02,
			O_CREAT  = 0100,
			O_EXCL   = 0200,
			O_TRUNC  = 01000,
			O_APPEND = 02000
		}

		public static int STDIN_FILENO = 0;
		public static int STDOUT_FILENO = 1;
		public static int STDERR_FILENO = 2;

		public StreamFS(IntPtr raw) : base(raw) { }

		[DllImport("camel-1.2")] static extern IntPtr camel_stream_fs_new_with_name(string name, int flags, int mode);
		[DllImport("camel-1.2")] static extern IntPtr camel_stream_fs_new_with_fd(int fd);

		public StreamFS(string name, Flags flags, int mode) : base((IntPtr)0) {
			cobject = camel_stream_fs_new_with_name(name, (int)flags, mode);
			toCamel(this);
		}

		public StreamFS(int fd) : base((IntPtr)0) {
			cobject = camel_stream_fs_new_with_fd(fd);
			toCamel(this);
		}
	}

	// this should obviously be extracted at build time
	[StructLayout (LayoutKind.Explicit)]
	struct CamelStreamMem {
		[FieldOffset(44)] public IntPtr buffer;
	}

	struct GByteArray {
		public IntPtr data;
		public int len;
	}

	struct GPtrArray {
		public IntPtr pdata;
		public int len;
	}

	public class StreamMem : Camel.SeekableStream {
		public StreamMem(IntPtr raw) : base(raw) { }

		[DllImport("camel-1.2")]
                static extern IntPtr camel_stream_mem_new();

		/* stupid c# */
		public StreamMem() : base((IntPtr)0) {
			cobject = camel_stream_mem_new();
			toCamel(this);
		}

		// should probably have some sort of interface for incremental/range gets too
		public Byte[] getBuffer() {
			CamelStreamMem mem = (CamelStreamMem)Marshal.PtrToStructure(cobject, typeof(CamelStreamMem));
			GByteArray ba = (GByteArray)Marshal.PtrToStructure(mem.buffer, typeof(GByteArray));
			Byte[] res = new Byte[ba.len];

			Marshal.Copy(ba.data, res, 0, ba.len);

			return res;
		}
	}

	// should do iterators etc?
	public class Address : Camel.Object {
		public Address(IntPtr raw) : base (raw) { }

		[DllImport("camel-1.2")] static extern IntPtr camel_address_new();
		[DllImport("camel-1.2")] static extern int camel_address_length(IntPtr raw);
		[DllImport("camel-1.2")] static extern int camel_address_decode(IntPtr raw, string addr);
		[DllImport("camel-1.2")] static extern string camel_address_encode(IntPtr raw);
		[DllImport("camel-1.2")] static extern int camel_address_unformat(IntPtr raw, string addr);
		[DllImport("camel-1.2")] static extern string camel_address_format(IntPtr raw);
		[DllImport("camel-1.2")] static extern int camel_address_cat(IntPtr raw, IntPtr src);
		[DllImport("camel-1.2")] static extern int camel_address_copy(IntPtr raw, IntPtr src);
		[DllImport("camel-1.2")] static extern void camel_address_remove(IntPtr raw, int index);

		public Address() : base((IntPtr)0) {
			cobject = camel_address_new();
			toCamel(this);
		}

		public int length() {
			return camel_address_length(cobject);
		}

		public void decode(string addr) {
			if (camel_address_decode(cobject, addr) == -1)
				throw new Exception(Exception.Type.SYSTEM, "Invalid address: " + addr);
		}

		public string encode() {
			return camel_address_encode(cobject);
		}

		public void unformat(string addr) {
			if (camel_address_unformat(cobject, addr) == -1)
				throw new Exception(Exception.Type.SYSTEM, "Invalid address: " + addr);
		}

		public string format() {
			return camel_address_format(cobject);
		}

		public void cat(Address from) {
			camel_address_cat(cobject, from.cobject);
		}

		public void copy(Address from) {
			camel_address_copy(cobject, from.cobject);
		}
	}

	public class InternetAddress : Camel.Address {
		public InternetAddress(IntPtr raw) : base (raw) { }
		
		[DllImport("camel-1.2")] static extern IntPtr camel_internet_address_new();
		[DllImport("camel-1.2")] static extern int camel_internet_address_add(IntPtr raw, string name, string addr);
		[DllImport("camel-1.2")] static extern bool camel_internet_address_get(IntPtr raw, out string name, out string addr);
		[DllImport("camel-1.2")] static extern int camel_internet_address_find_name(IntPtr raw, string name, out string addr);
		[DllImport("camel-1.2")] static extern int camel_internet_address_find_address(IntPtr raw, string addr, out string name);
		[DllImport("camel-1.2")] static extern string camel_internet_address_encode_address(out int len, string name, string addr);
		[DllImport("camel-1.2")] static extern string camel_internet_address_format_address(string name, string addr);
		
		public InternetAddress() : base((IntPtr)0) {
			cobject = camel_internet_address_new();
			toCamel(this);
		}

		public void add(string name, string addr) {
			camel_internet_address_add(cobject, name, addr);
		}

		public bool get(out string name, out string addr) {
			name = null;
			addr = null;
			return camel_internet_address_get(cobject, out name, out addr);
		}

		// this is a weird arsed interface ...
		public int findName(string name, out string addr) {
			addr = null;
			// FIXME: addr is const, need to marshal to local
			return camel_internet_address_find_name(cobject, name, out addr);
		}

		public int findAddress(string addr, out string name) {
			name = null;
			return camel_internet_address_find_name(cobject, addr, out name);
		}

		public static string encode(string name, string addr) {
			int len = 0;
			// another weird-arsed interface
			return camel_internet_address_encode_address(out len, name, addr);
		}

		public static string format(string name, string addr) {
			return camel_internet_address_format_address(name, addr);
		}
	}

	public class MessageInfo {
		public IntPtr cobject;
		private Tags user_tags;
		private Flags user_flags;

		private enum Type {
			SUBJECT,
			FROM,
			TO,
			CC,
			MLIST,

			FLAGS,
			SIZE,

			DATE_SENT,
			DATE_RECEIVED,

			MESSAGE_ID,
			REFERENCES,

			USER_FLAGS,
			USER_TAGS,

			LAST,
		}

		public class Tags {
			private MessageInfo mi;

			[DllImport("camel-provider-1.2")] static extern IntPtr camel_message_info_user_tag(IntPtr mi, String name);
			[DllImport("camel-provider-1.2")] static extern bool camel_message_info_set_user_tag(IntPtr mi, String name, String value);

			public Tags(MessageInfo raw) {
				mi = raw;
			}

			public String this [String tag] {
				get {
					return Marshal.PtrToStringAnsi(camel_message_info_user_tag(mi.cobject, tag));
				}
				set {
					camel_message_info_set_user_tag(mi.cobject, tag, value);
				}
			}
		}

		public class Flags {
			private MessageInfo mi;

			[DllImport("camel-provider-1.2")] static extern bool camel_message_info_user_flag(IntPtr miptr, String name);
			[DllImport("camel-provider-1.2")] static extern bool camel_message_info_set_user_flag(IntPtr miptr, String name, bool value);

			// note raw is a pointer to a pointer of tags
			public Flags(MessageInfo raw) {
				mi = raw;
			}

			public bool this [String tag] {
				get {
					return camel_message_info_user_flag(mi.cobject, tag);
				}
				set {
					camel_message_info_set_user_flag(mi.cobject, tag, value);
				}
			}
		}

		// only used to calculate offsets
		private struct CamelMessageInfo {
			IntPtr summary;
			uint refcount;
			string uid;
		};

		public MessageInfo(IntPtr raw) {
			cobject = raw;
		}

		[DllImport("camel-provider-1.2")] static extern void camel_folder_free_message_info(IntPtr raw, IntPtr info);
		[DllImport("camel-provider-1.2")] static extern void camel_message_info_free(IntPtr info);

		~MessageInfo() {
			camel_message_info_free(cobject);
		}

		[DllImport("camel-provider-1.2")] static extern IntPtr camel_message_info_ptr(IntPtr raw, int type);
		[DllImport("camel-provider-1.2")] static extern uint camel_message_info_uint32(IntPtr raw, int type);
		[DllImport("camel-provider-1.2")] static extern uint camel_message_info_time(IntPtr raw, int type);

		public String uid { get { return Marshal.PtrToStringAuto(Marshal.ReadIntPtr(cobject, (int)Marshal.OffsetOf(typeof(CamelMessageInfo), "uid"))); } }

		public String subject { get { return Marshal.PtrToStringAnsi(camel_message_info_ptr(cobject, (int)Type.SUBJECT)); } }
		public String from { get { return Marshal.PtrToStringAnsi(camel_message_info_ptr(cobject, (int)Type.FROM)); } }
		public String to { get { return Marshal.PtrToStringAnsi(camel_message_info_ptr(cobject, (int)Type.TO)); } }
		public String cc { get { return Marshal.PtrToStringAnsi(camel_message_info_ptr(cobject, (int)Type.CC)); } }
		public String mlist { get { return Marshal.PtrToStringAnsi(camel_message_info_ptr(cobject, (int)Type.MLIST)); } }

		public uint flags { get { return camel_message_info_uint32(cobject, (int)Type.FLAGS); } }
		public uint size { get { return camel_message_info_uint32(cobject, (int)Type.SIZE); } }

		public Tags userTags {
			get {
				if (user_tags == null)
					user_tags = new Tags(this);
				return user_tags;
			}
		}

		public Flags userFlags {
			get {
				if (user_flags == null)
					user_flags = new Flags(this);
				return user_flags;
			}
		}
	}

	public class URL {
		public IntPtr cobject;
		internal Params param_list;

		// we never instantiate this, we just use it to describe the layout
		internal struct CamelURL {
			internal IntPtr protocol;
			internal IntPtr user;
			internal IntPtr authmech;
			internal IntPtr passwd;
			internal IntPtr host;
			internal int    port;
			internal IntPtr path;
			internal IntPtr pparams;
			internal IntPtr query;
			internal IntPtr fragment;
		};

		public class Params {
			private URL parent;

			internal Params(URL _parent) {
				parent = _parent;
			}

			public string this[string name] {
				set { camel_url_set_param(parent.cobject, name, value); }
				get { return Marshal.PtrToStringAnsi(camel_url_get_param(parent.cobject, name)); }
			}
		}

		[DllImport("camel-1.2")] static extern IntPtr camel_url_new_with_base(IntPtr bbase, string url);
		[DllImport("camel-1.2")] static extern IntPtr camel_url_new(string url, ref CamelException ex);
		[DllImport("camel-1.2")] static extern string camel_url_to_string(IntPtr url, int flags);
		[DllImport("camel-1.2")] static extern void camel_url_free(IntPtr url);

		// this is a shit to wrap, needs accessors or other pain
		[DllImport("camel-1.2")] static extern void camel_url_set_protocol(IntPtr url, string s);
		[DllImport("camel-1.2")] static extern void camel_url_set_user(IntPtr url, string s);
		[DllImport("camel-1.2")] static extern void camel_url_set_authmech(IntPtr url, string s);
		[DllImport("camel-1.2")] static extern void camel_url_set_passwd(IntPtr url, string s);
		[DllImport("camel-1.2")] static extern void camel_url_set_host(IntPtr url, string s);
		[DllImport("camel-1.2")] static extern void camel_url_set_port(IntPtr url, int p);
		[DllImport("camel-1.2")] static extern void camel_url_set_path(IntPtr url, string s);
		[DllImport("camel-1.2")] static extern void camel_url_set_param(IntPtr url, string s, string v);
		[DllImport("camel-1.2")] static extern void camel_url_set_query(IntPtr url, string s);
		[DllImport("camel-1.2")] static extern void camel_url_set_fragment(IntPtr url, string s);

		[DllImport("camel-1.2")] static extern IntPtr camel_url_get_param(IntPtr url, string s);

		[DllImport("camel-1.2")] static extern string camel_url_encode(string url, string escape);
		// ugh we can't do this, it writes to its result??
		// -> use StringBuilder
		[DllImport("camel-1.2")] static extern IntPtr camel_url_decode(ref string url);

		public URL(string uri) {
			CamelException ex = new CamelException();

			cobject = camel_url_new(uri, ref ex);
			if (ex.id != 0)
				throw new Exception(ex);
		}

		public URL(URL bbase, string uri) {
			cobject = camel_url_new_with_base(bbase.cobject, uri);
		}

		~URL() {
			camel_url_free(cobject);
		}

		/* its ugly but it works */
		private string field(string name) {
			return Marshal.PtrToStringAuto(Marshal.ReadIntPtr(cobject, (int)Marshal.OffsetOf(typeof(CamelURL), name)));
		}

		public string protocol {
			set { camel_url_set_protocol(cobject, value); }
			get { return field("protocol"); }
		}

		public string user {
			set { camel_url_set_user(cobject, value); }
			get { return field("user"); }
		}

		public string authmech {
			set { camel_url_set_authmech(cobject, value); }
			get { return field("authmech"); }
		}

		public string passwd {
			set { camel_url_set_passwd(cobject, value); }
			get { return field("passwd"); }
		}

		public string host {
			set { camel_url_set_host(cobject, value); }
			get { return field("host"); }
		}

		public int port {
			set { camel_url_set_port(cobject, value); }
			get { return (int)Marshal.ReadIntPtr(cobject, (int)Marshal.OffsetOf(typeof(CamelURL), "port")); }
		}

		public string path {
			set { camel_url_set_path(cobject, value); }
			get { return field("path"); }
		}

		public string query {
			set { camel_url_set_query(cobject, value); }
			get { return field("query"); }
		}

		public string fragment {
			set { camel_url_set_fragment(cobject, value); }
			get { return field("fragment"); }
		}

		public Params paramlist {
			get {
				if (param_list == null)
					param_list = new Params(this);
				return param_list;
			}
		}

		public override string ToString() {
			return camel_url_to_string(cobject, 0);
		}

		public static string encode(string val) {
			return camel_url_encode(val, null);
		}

		public static string encode(string val, string escape) {
			return camel_url_encode(val, escape);
		}
	}
}

namespace Camel.Hash {
	public class Stream : System.IO.Stream {
		protected Camel.Stream substream;

		public Stream(Camel.Stream sub) {
			substream = sub;
		}

		public override bool CanSeek { get { return false; } }
		public override bool CanRead { get { return true; } }
		public override bool CanWrite { get { return true; } }
		public override long Length {
			get {
				throw new System.IO.IOException("Cannot get stream length");
			}
		}
		public override long Position {
			get {
				throw new System.IO.IOException("Cannot get stream position");
			}
			set {
				if (value == 0) {
					substream.reset();
				} else {
					throw new System.IO.IOException("Cannot set stream position");
				}
			}
		}
	
		public override int Read(byte[] buffer, int offset, int count) {
			// FIXME: how to add the offset to the buffer?
			return substream.read(buffer, count);
		}

		public override void Write(byte[] buffer, int offset, int count) {
			// FIXME: how to add the offset to the buffer?
			substream.write(buffer, count);
		}

		public override void Flush() {
			substream.flush();
		}

		public override long Seek(long offset, System.IO.SeekOrigin seek) {
			throw new System.IO.IOException("Seeking not supported");
		}

		public override void SetLength(long len) {
			throw new System.IO.IOException("Cannot set stream length");
		}
	}
}

/*
namespace Evolution.Mail {
	class Component : GLib.Object {
		public Component(IntPtr raw) : base(raw) {}
		public Component() : base() {}

		~Component() {
			Dispose();
		}

		[DllImport("libevolution-mail.so")] static extern IntPtr mail_component_peek();
		[DllImport("libevolution-mail.so")] static extern IntPtr mail_component_peek_base_directory(IntPtr component);
		[DllImport("libevolution-mail.so")] static extern IntPtr mail_component_peek();

		public static Component peek() {
			return new Component(mail_component_peek());
		}

		public String baseDirectory {
			get {}
		}
}
*/
