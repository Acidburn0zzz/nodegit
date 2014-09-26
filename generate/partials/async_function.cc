/**
{%partial doc .%}
  */
NAN_METHOD({{ cppClassName }}::{{ cppFunctionName }}) {
  NanScope();
  {%partial guardArguments .%}

  if (args.Length() == {{args|jsArgsCount}} || !args[{{args|jsArgsCount}}]->IsFunction()) {
    return NanThrowError("Callback is required and must be a Function.");
  }

  {{ cppFunctionName }}Baton* baton = new {{ cppFunctionName }}Baton;
  baton->error_code = GIT_OK;
  baton->error = NULL;
  {%each args|argsInfo as arg %}
    {%if not arg.isReturn %}
      {%if arg.isSelf %}
  baton->{{ arg.name }} = ObjectWrap::Unwrap<{{ arg.cppClassName }}>(args.This())->GetValue();
      {%elsif arg.name $}
  {%partial convertFromV8 .%}
        {%if not arg.isPayload %}
  baton->{{ arg.name }} = from_{{ arg.name }};
        {%endif}
      {%endif%}
    {%elsif arg.shouldAlloc %}
  baton->{{ arg.name }} = ({{ arg.cType }})malloc(sizeof({{ arg.cType|replace '*' '' }}));
    {%endif%}
  {%endeach%}

  NanCallback *callback = new NanCallback(Local<Function>::Cast(args[{{args|jsArgsCount}}]));
  {{ cppFunctionName }}Worker *worker = new {{ cppFunctionName }}Worker(baton, callback);
  {%each args|argsInfo as arg %}
    {%if not arg.isReturn %}
      {%if arg.isSelf %}
  worker->SaveToPersistent("{{ arg.name }}", args.This());
      {%else%}
  if (!args[{{ arg.jsArg }}]->IsUndefined() && !args[{{ arg.jsArg }}]->IsNull())
  {
    worker->SaveToPersistent("{{ arg.name }}", args[{{ arg.jsArg }}]->ToObject());
  }
      {%endif%}
    {%endif%}
  {%endeach%}

  NanAsyncQueueWorker(worker);
  NanReturnUndefined();
}

void {{ cppClassName }}::{{ cppFunctionName }}Worker::Execute() {
  {%if functionInfo|hasReturnType %}
  {{ return.cType }} result =
  {%endif%}
  {{ cFunctionName }}(
    {%-- Insert Function Arguments --%}
    {%each args|argsInfo as arg %}
      {%-- turn the pointer into a ref %}
    {%if arg.isReturn|and arg.cType|isPointer %}&{%endif%}baton->{{ arg.name }}{%if not arg.lastArg %},{%endif%}
    {%endeach%}
    );

  {%if return.isErrorCode %}
  baton->error_code = result;

  if (result != GIT_OK && giterr_last() != NULL) {
    baton->error = git_error_dup(giterr_last());
  }

  {%elsif not return.cType == 'void' %}

  baton->result = result;

  {%endif%}
}

void {{ cppClassName }}::{{ cppFunctionName }}Worker::HandleOKCallback() {
  TryCatch try_catch;
  if (baton->error_code == GIT_OK) {
    {%if not returns.length %}
  Handle<Value> result = NanUndefined();
    {%else%}
      {%each returns|converReturns as _return %}
        {%if returns.length == 1 %}
  Handle<Value> to;
  {%partial convertToV8 _return %}
  Handle<Value> result = to;
        {%else%}
  Handle<Object> result = NanNew<Object>();
  Handle<Value> to;
  {%partial convertToV8 _return %}
  result->Set(NanNew<String>("{{ _return.jsName | or _return.name }}"), to);
        {%endif%}
      {%endeach%}
    {%endif%}
    Handle<Value> argv[2] = {
      NanNull(),
      result
    };
    callback->Call(2, argv);
  } else {
    if (baton->error) {
      Handle<Value> argv[1] = {
        NanError(baton->error->message)
      };
      callback->Call(1, argv);
      if (baton->error->message)
        free((void *)baton->error->message);
      free((void *)baton->error);
    } else {
      callback->Call(0, NULL);
    }

    {%each args as arg %}
      {%if arg.shouldAlloc %}
    free(baton->{{ arg.name }});
      {%endif%}
    {%endeach%}
  }

  if (try_catch.HasCaught()) {
    node::FatalException(try_catch);
  }

  {%each args as arg %}
    {%if arg.cppClassName == 'String' | or arg.cppClassName == 'Array' %}
      {%if arg.freeFunctionName %}
  {{ arg.freeFunctionName }}(baton->{{ arg.name }});
      {%else%}
  free((void *)baton->{{ arg.name }});
      {%endif%}
    {%endif%}
  {%endeach%}

  delete baton;
}