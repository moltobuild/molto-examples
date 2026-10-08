CREATE TABLE items (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name varchar(120) NOT NULL CHECK (length(btrim(name)) > 0),
    price numeric(12, 2) NOT NULL CHECK (price >= 0),
    stock integer NOT NULL CHECK (stock >= 0),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    updated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);

CREATE FUNCTION touch_item_updated_at() RETURNS trigger LANGUAGE plpgsql AS $$
BEGIN
    NEW.created_at = OLD.created_at;
    NEW.updated_at = clock_timestamp();
    RETURN NEW;
END;
$$;

CREATE TRIGGER items_updated_at BEFORE UPDATE ON items
FOR EACH ROW EXECUTE FUNCTION touch_item_updated_at();
